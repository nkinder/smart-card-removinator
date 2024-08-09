// Copyright 2016 Nathan Kinder and the Smart Card Removinator contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPI is used to communicate with shift registers.
#include <SPI.h>
#include <EEPROM.h>

// Serial command syntax
#define CMD_PREFIX '#'
#define CMD_TERM '\r'
#define CMD_MAX_LEN 7

// Serial commands
#define CMD_CARD_N "SC"
#define CMD_REMOVE_CARD "REM"
#define CMD_LOCK_N "LOCK"
#define CMD_UNLOCK_N "UNLOCK"
#define CMD_STATUS "STA"
#define CMD_DEBUG "DBG"
#define CMD_HELP "?"

// Standard serial responses
#define RESP_OK "OK"
#define RESP_DBG_ON "DBG_ON"
#define RESP_DBG_OFF "DBG_OFF"
#define ERR_UNKNOWN_CMD "ERR_UNKNOWN_CMD"
#define ERR_CMDLEN "ERR_CMDLEN"
#define ERR_PROTOCOL "ERR_PROTOCOL"
#define ERR_NOCARD "ERR_NOCARD"
#define ERR_LOCKED "ERR_LOCKED"
#define ERR_BADIDX "ERR_BADIDX"

// Debug output defines
#define LOG_DBG_PREFIX "[DBG] "

// Constants
unsigned const char display_digits[] = {
    0x84, 0xD7, 0xA1, 0x91, 0xD2,
    0x98, 0x88, 0xD5, 0x80, 0x90,
};
const int mux_en_pin = 2;
const int mux_a0_pin = 3;
const int mux_a1_pin = 4;
const int mux_a2_pin = 5;
const int display_select_pin = 6;
const int socket_switch_select_pin = 7;
const int reader_switch_pin = 8;
unsigned const char eeprom_magic[] = { 0xcb, 0x21, 0xc7, 0x8a };
const int eeprom_size = 512;  // including the magic above

// Globals
char command[CMD_MAX_LEN + 1];
int inserted_card = 0;
bool debug = false;

// Function prototypes
void setup();
void loop();
int getCommand();
int insertCard(int card);
void removeCard(bool print_response);
int lockCard(int card);
void unlockCard(int card);
bool validEEPROM();
unsigned char getLockBitmap();
void setLockBitmap(unsigned char value);
int updateDisplay(int digit);
void printCardStatus();
unsigned char getCardStatus(int card);
void toggleDebug();
void debug_print(String msg);
void usage();


void setup()
{
    // Set up pins.
    pinMode(mux_en_pin, OUTPUT);
    pinMode(mux_a0_pin, OUTPUT);
    pinMode(mux_a1_pin, OUTPUT);
    pinMode(mux_a2_pin, OUTPUT);
    pinMode(display_select_pin, OUTPUT);
    pinMode(socket_switch_select_pin, OUTPUT);
    pinMode(reader_switch_pin, OUTPUT);

    // Set SPI slave select pins high to ignore master.  We will
    // enable SPI devices as needed.
    digitalWrite(display_select_pin, HIGH);
    digitalWrite(socket_switch_select_pin, HIGH);

    // Start the SPI library.
    SPI.begin();

    // Start with no card inserted.
    removeCard(false);

    // Set up the serial port.
    Serial.begin(9600);
}

// return true if a regular command fully matches
#define CMD_MATCHES(arg) (strcmp(command, arg) == 0)
// return true if an 'indexed' (cmd with a card index) command matches
#define INDEXED_CMD_MATCHES(arg) \
    (strncmp(command, arg, sizeof(arg)-1) == 0 && strlen(command) == sizeof(arg))
// return a string starting at the index argument of the cmd
#define CMD_INDEX(arg) (command + sizeof(arg) - 1)
// ensure the index is an ASCII number between '1' and '8'
#define VALID_INDEX(arg) (*arg >= 0x31 && *arg <= 0x38)

void loop()
{
    char *index;

    // Attempt to get a command from the serial port
    if (getCommand() == 0) {
        debug_print(String("Received command \"" + String(command) + "\""));
        if (INDEXED_CMD_MATCHES(CMD_CARD_N)) {
            index = CMD_INDEX(CMD_CARD_N);
            if (VALID_INDEX(index)) {
                insertCard(atoi(index));
            } else {
                Serial.println(ERR_BADIDX);
            }
        } else if (CMD_MATCHES(CMD_REMOVE_CARD)) {
            removeCard(true);
        } else if (INDEXED_CMD_MATCHES(CMD_LOCK_N)) {
            index = CMD_INDEX(CMD_LOCK_N);
            if (VALID_INDEX(index)) {
                lockCard(atoi(index));
            } else {
                Serial.println(ERR_BADIDX);
            }
        } else if (INDEXED_CMD_MATCHES(CMD_UNLOCK_N)) {
            index = CMD_INDEX(CMD_UNLOCK_N);
            if (VALID_INDEX(index)) {
                unlockCard(atoi(index));
            } else {
                Serial.println(ERR_BADIDX);
            }
        } else if (CMD_MATCHES(CMD_STATUS)) {
            printCardStatus();
        } else if (CMD_MATCHES(CMD_HELP)) {
            usage();
        } else if (CMD_MATCHES(CMD_DEBUG)) {
            toggleDebug();
        } else {
            Serial.println(ERR_UNKNOWN_CMD);
        }

        // Clear the previous command
        command[0] = '\0';
    }
}

// getCommand()
//
// Get a command from the serial port, checking for a proper envelope and length.
//
// Returns:
//          0 - If we received a properly enveloped command.
//   Non-zero - If we received an illegally formatted command.  A more
//              detailed error code will be returned via serial.
//
int getCommand()
{
    int rc = 1;
    char inbyte = 0;
    int  command_length = 0;

    if (Serial.available() > 0) {
        inbyte = Serial.read();

        // Make sure the command start sequence is present.
        if (inbyte == CMD_PREFIX) {
            while ((inbyte != CMD_TERM) && (command_length <= CMD_MAX_LEN)) {
                inbyte = Serial.read();
                if ((inbyte > 0) && (inbyte != CMD_TERM)) {
                    command[command_length] = inbyte;
                    command_length++;
                }
            }

            // See if we exceeded the maximum command length.
            if (inbyte != CMD_TERM) {
                Serial.println(ERR_CMDLEN);
                command[0] = '\0';
            } else {
                // Terminate the command string.
                command[command_length] = '\0';
                rc = 0;
            }
        } else {
            // Illegally formatted command or garbage (protocol error).
            Serial.println(ERR_PROTOCOL);
        }
    }

    return rc;
}

// insertCard()
//
// Inserts the specified card.  If another card is already inserted, it will be removed.
//
// One of the card sockets is switched to the card reader output by controlling
// a set of MAX4638 8:1 analog multiplexor ICs.  There is one mux IC per card contact.
// The mux logic pins are tied together in parallel so that they are switched at the
// same time.  This allows us to control all of the mux ICs using only 4 pins.  The
// switching logic uses the following truth table:
//
//     --------------------------------
//     | A2  | A1  | A0  | EN  |  ON  |
//     --------------------------------
//     |  0  |  0  |  0  |  0  | NONE |
//     --------------------------------
//     |  0  |  0  |  0  |  1  |  NO1 |
//     --------------------------------
//     |  0  |  0  |  1  |  1  |  NO2 |
//     --------------------------------
//     |  0  |  1  |  0  |  1  |  NO3 |
//     --------------------------------
//     |  0  |  1  |  1  |  1  |  NO4 |
//     --------------------------------
//     |  1  |  0  |  0  |  1  |  NO5 |
//     --------------------------------
//     |  1  |  0  |  1  |  1  |  NO6 |
//     --------------------------------
//     |  1  |  1  |  0  |  1  |  NO7 |
//     --------------------------------
//     |  1  |  1  |  1  |  1  |  NO8 |
//     --------------------------------
//
// Parameters:
//     card - The card number to insert (1-8).
//
// Returns:
//          0 - Success.
//   Non-zero - If we had some problem inserting the card.  A more
//              detailed error code will be returned via serial.
int insertCard(int card)
{
    // Check if the requested card is actually present.  If not, return
    // ERR_NOCARD via serial and don't do any switching.
    if (!getCardStatus(card)) {
        Serial.println(ERR_NOCARD);
        return 1;
    }

    // Check if the requested card slot is locked.
    unsigned char bitmap = getLockBitmap();
    if (bitmap & (1 << (card - 1))) {
        Serial.println(ERR_LOCKED);
        return 1;
    }

    // First remove the existing card.  This will cycle the reader switch
    // pin to ensure that the reader actually detects a removal/insertion
    // event.  We give this a slight delay to ensure it triggers an event
    // in the card reader driver.
    removeCard(false);
    delay(200);

    // Set the logic pins to select the requested card according to
    // the truth table.  We disable the muxes first to ensure we switch
    // directly to the requested card in cases where we have to change
    // the state of multiple pins.
    switch(card) {
        case 1:
            digitalWrite(mux_en_pin, LOW);
            digitalWrite(mux_a2_pin, LOW);
            digitalWrite(mux_a1_pin, LOW);
            digitalWrite(mux_a0_pin, LOW);
            digitalWrite(mux_en_pin, HIGH);
            break;
        case 2:
            digitalWrite(mux_en_pin, LOW);
            digitalWrite(mux_a2_pin, LOW);
            digitalWrite(mux_a1_pin, LOW);
            digitalWrite(mux_a0_pin, HIGH);
            digitalWrite(mux_en_pin, HIGH);
            break;
        case 3:
            digitalWrite(mux_en_pin, LOW);
            digitalWrite(mux_a2_pin, LOW);
            digitalWrite(mux_a1_pin, HIGH);
            digitalWrite(mux_a0_pin, LOW);
            digitalWrite(mux_en_pin, HIGH);
            break;
        case 4:
            digitalWrite(mux_en_pin, LOW);
            digitalWrite(mux_a2_pin, LOW);
            digitalWrite(mux_a1_pin, HIGH);
            digitalWrite(mux_a0_pin, HIGH);
            digitalWrite(mux_en_pin, HIGH);
            break;
        case 5:
            digitalWrite(mux_en_pin, LOW);
            digitalWrite(mux_a2_pin, HIGH);
            digitalWrite(mux_a1_pin, LOW);
            digitalWrite(mux_a0_pin, LOW);
            digitalWrite(mux_en_pin, HIGH);
            break;
        case 6:
            digitalWrite(mux_en_pin, LOW);
            digitalWrite(mux_a2_pin, HIGH);
            digitalWrite(mux_a1_pin, LOW);
            digitalWrite(mux_a0_pin, HIGH);
            digitalWrite(mux_en_pin, HIGH);
            break;
        case 7:
            digitalWrite(mux_en_pin, LOW);
            digitalWrite(mux_a2_pin, HIGH);
            digitalWrite(mux_a1_pin, HIGH);
            digitalWrite(mux_a0_pin, LOW);
            digitalWrite(mux_en_pin, HIGH);
            break;
        case 8:
            digitalWrite(mux_en_pin, LOW);
            digitalWrite(mux_a2_pin, HIGH);
            digitalWrite(mux_a1_pin, HIGH);
            digitalWrite(mux_a0_pin, HIGH);
            digitalWrite(mux_en_pin, HIGH);
            break;
        default:
            debug_print(String("Unexpected card in insertCard(): " + String(card)));
            return 1;
        break;
    }

    // Set the reader switch pin to low to trigger an insertion
    // event.
    digitalWrite(reader_switch_pin, LOW);

    // Update the display to show the inserted card number.
    updateDisplay(card);
    debug_print(String("Inserted card " + String(card)));
    inserted_card = card;

    // Return our serial response.
    Serial.println(RESP_OK);

    return 0;
}

// removeCard()
//
// Remove the inserted card.  If a card is not inserted,
// this is just a no-op.
//
// We "remove" a card by disabling the multiplexor ICs by setting
// their enable pin to low.  This disconnects the card contacts from
// the card reader adapter.  We also set the reader switch pin to HIGH
// so the reader thinks that the card was physically removed.
//
// Parameters:
//     print_response - Print response to the serial port if TRUE.  We
//                      call this function internally and only want to
//                      print a response if the user explicitly sent a
//                      command to remove the card.
//
void removeCard(bool print_response)
{
    digitalWrite(reader_switch_pin, HIGH);
    digitalWrite(mux_en_pin, LOW);
    debug_print(String("Removed card " + String(inserted_card)));
    inserted_card = 0;

    // Blank the display
    updateDisplay(-1);

    // Print our serial response if requested.
    if (print_response) {
        Serial.println(RESP_OK);
    }
}

// lockCard()
//
// Locks the specified card slot, making another lock operation on the same slot
// fail if attempted. An unlock must be issued on the slot to restore successful
// locking functionality.
//
// The idea is to limit accidental use in unexpected scenarios, bricking cards
// by a client entering a wrong password repeatedly due to an automation bug.
// The client can lock a slot before using it and unlock it only if everything
// went smoothly. Any infrastructure crash or automation bug should leave it
// locked, letting a human investigate the situation and manually reset the card
// password attempts using an external card reader and a correct password.
//
// Parameters:
//     card - The card number to lock (1-8).
//
// Returns:
//     0 - Success
//     Non-zero - Error
//
int lockCard(int card)
{
    unsigned char bitmap = getLockBitmap();
    int index = card - 1;

    // Don't allow locking an empty slot.
    if (!getCardStatus(card)) {
        Serial.println(ERR_NOCARD);
        return 1;
    }

    // Refuse to lock an already locked slot.
    if (bitmap & (1 << index)) {
        Serial.println(ERR_LOCKED);
        return 1;
    }

    bitmap |= 1 << index;
    setLockBitmap(bitmap);

    Serial.println(RESP_OK);
    debug_print(String("Locked card " + String(card)));

    return 0;
}

// unlockCard()
//
// Unlocks the specified card slot, see lockCard().
//
// Parameters:
//     card - The card number to unlock (1-8).
//
void unlockCard(int card)
{
    unsigned char bitmap = getLockBitmap();
    bitmap &= ~(1 << (card - 1));
    setLockBitmap(bitmap);

    Serial.println(RESP_OK);
    debug_print(String("Unlocked card " + String(card)));
}

// validEEPROM()
//
// Returns true if the EEPROM contains valid bitmap data written by us,
// false otherwise.
//
bool validEEPROM()
{
    unsigned int i;
    for (i = 0; i < sizeof(eeprom_magic) / sizeof(unsigned char); i++) {
        if (EEPROM.read(i) != eeprom_magic[i]) {
            return false;
        }
    }
    return true;
}

// getLockBitmap()
//
// Returns an 8-bit bitmap with bit indices representing card slots,
// and bit values locked (1) or unlocked (0) status.
//
// The EEPROM wear-leveling uses a simple linear traversal, using 2-byte
// chunks to represent a usage flag (first byte) and the lock bitmap
// (second byte).
//     0x04    0x05    0x06    0x07    0x08    0x09    (address)
//   +------------------------------------------------------------
//   | flag | bitmap | flag | bitmap | flag | bitmap | ...
//   +------------------------------------------------------------
// The usage flag can be either 0x00 or 0xff, and the place at which it
// flips from one value to the other is the last-written spot.
// If all usage flags are the same, the last chunk should be used.
//
unsigned char getLockBitmap()
{
    unsigned int i;

    if (!validEEPROM()) {
        // We have never written to EEPROM - consider everything unlocked.
        return 0x00;
    }

    int start_addr = sizeof(eeprom_magic) / sizeof(unsigned char);

    for (i = start_addr; i < eeprom_size-3; i += 2) {
        if (EEPROM.read(i) != EEPROM.read(i+2)) {
            return EEPROM.read(i+1);
        }
    }

    // The flag never changed, but EEPROM magic signature was present,
    // which means the last written chunk was on the last possible index.
    return EEPROM.read(i+1);
}

// setLockBitmap()
//
// Stores an 8-bit bitmap in the EEPROM, as a counterpart to getLockBitmap().
//
// Parameters:
//     value - The bitmap value to be stored in EEPROM.
//
void setLockBitmap(unsigned char value)
{
    unsigned int i;

    if (!validEEPROM()) {
        // We have never written to EEPROM - initialize it.
        for (i = 0; i < sizeof(eeprom_magic) / sizeof(unsigned char); i++) {
            EEPROM.write(i, eeprom_magic[i]);
        }
        for (; i < eeprom_size; i++) {
            EEPROM.write(i, 0x00);
        }
    }

    int start_addr = sizeof(eeprom_magic) / sizeof(unsigned char);

    // Find the last used chunk.
    for (i = start_addr; i < eeprom_size-3; i += 2) {
        if (EEPROM.read(i) != EEPROM.read(i+2)) {
            break;
        }
    }

    // Write a new value to the next chunk.
    int new_index = i < eeprom_size-3 ? i+2 : start_addr;
    unsigned char new_flag = ~EEPROM.read(new_index);
    EEPROM.write(new_index, new_flag);
    EEPROM.write(new_index+1, value);
}

// updateDisplay()
//
// Updates the display to the requested digit.
//
// The display is a 7-segment display controlled via a SN74HC595 shift
// register, which we control via SPI.  We send the bits in least-significant
// bit order, meaning that output 'A' of the shift register is controlled by
// the LSB and output 'H' is controlled by the MSB.  The shift register
// outputs are connected to the 7-segment display as follows:
//
//       B
//     -----
//   A |   | D
//   C -----
//   E |   | F
//     -----
//       G
//
// Additionally, there is an LED for a decimal point, which is controlled by
// output 'H' of the shift register.
//
// Parameters:
//     digit - Digit to display.  Valid values are 0-9.  A value
//             of -1 will cause the display to be cleared.
//
// Returns:
//     0 - Success
//     Non-zero - Error due to invalid digit parameter.
//
int updateDisplay(int digit)
{
    int rc = 0;

    // Start a SPI transaction with a max speed of 20MHz.
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

    // Select the display device.
    digitalWrite(display_select_pin, LOW);

    // Display the requested digit by turning on the proper
    // LED segments.
    if ((digit >= 0) && (digit <= 9)) {
        SPI.transfer(display_digits[digit]);
    } else if(digit == -1) {
        // Blank the display, but leave the decimal point LED
        // on as a power indicator.
        SPI.transfer(0x7F);
    } else {
        debug_print(String("Unexpected digit in updateDisplay(): " + String(digit)));
        rc = 1;
    }

    // De-select the display device.  This rising edge pulse will trigger
    // an update to the actual display based off of the shift register contents.
    digitalWrite(display_select_pin, HIGH);

    // End the SPI transaction.
    SPI.endTransaction();

    if (rc == 0) {
        debug_print(String("Updated display to  " + String(digit)));
    }

    return rc;
}

// printCardStatus()
//
// Print the status to the serial port.  This includes which card socket
// is currently "inserted", along with status on which card sockets have
// an actual card present.
//
// The status is printed via serial as a JSON object.  It is
// comprised of two attributes.  The "current" attribute contains
// the number of the currently selected card.  The "present" attribute
// contains an array containing the ascending card socket numbers that
// have a card present.  Here is an example where card 4 is currently
// selected, and card sockets 1-6 have cards present:
//
//     {"current":4,"present":[1,2,3,4,5,6]}
//
// If no card is currently selected, the "current" card will be reported
// as 0.
//
void printCardStatus()
{
    int i = 0;
    int need_comma = 0;
    unsigned char status = 0;
    String status_json = "{";

    // Add the currently inserted card to the status.
    status_json += "\"current\":";
    status_json += inserted_card;

    // Add any present cards to the JSON array.
    status = getCardStatus(0);
    status_json += ",\"present\":[";
    for (i = 1; i <=8; i++) {
        if (((status >> (i - 1)) & 1) ^ 1) {
            if (need_comma) {
                status_json += ",";
            }
            status_json += i;
            need_comma = 1;
        }
    }
    status_json += "]";

    // Add currently locked card slots.
    need_comma = 0;
    status = getLockBitmap();
    status_json += ",\"locked\":[";
    for (i = 0; i < 8; i++) {
        if (status & (1 << i)) {
            if (need_comma) {
                status_json += ",";
            }
            // Cards/slots are 1-8, not 0-7
            status_json += i+1;
            need_comma = 1;
        }
    }
    status_json += "]";

    // Terminate the JSON status.
    status_json += "}";

    // Return the status via serial.
    Serial.println(status_json);

    // Terminate our serial response.
    Serial.println(RESP_OK);
}

// getCardStatus()
//
// Check which card sockets actually have cards inserted.
//
// Each card socket has a switch, which we read via a SN74HC165 shift
// register that we control via SPI.  This allows us to read all 8
// switches at once without taking up 8 input pins.
//
// Paramters:
//   card - The card socket to get the status of.  Valid values
//          are 0-8.  If card is 0, we check the status of all
//          card sockets.
//
// Returns:
//   status - A char indicating the status for the requested
//            card sockets.  If a single card socket is being
//            checked, this byte will be 0x01 if a card is
//            inserted, and 0x00 if the socket is empty.  If
//            all card sockets are being checked, each bit in
//            the returned byte will represent the status of
//            its associated card socket.  Card socket 1 is
//            represented by the LSB, and card socket 8 is
//            represented by the MSB.
//
unsigned char getCardStatus(int card)
{
    unsigned char status = 0;

    // Start a SPI transaction with a max speed of 20MHz.
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

    // Pulse the PISO register IC to make it load the card
    // socket switch values.
    digitalWrite(socket_switch_select_pin, LOW);
    delayMicroseconds(20);
    digitalWrite(socket_switch_select_pin, HIGH);

    // Retrieve the switch values via serial.
    status = SPI.transfer(0);
    debug_print(String("Received status: " + String(status)));

    // End the SPI transaction.
    SPI.endTransaction();

    // If we're checking the status of a single card, adjust the
    // returned status to be 1 or 0.
    if ((card >= 1) && (card <= 8)) {
        status = ((status >> (card - 1)) & 1) ^ 1;
    }

    return status;
}

// toggleDebug()
//
// Enables or disables debugging serial output.
//
void toggleDebug()
{
    debug = !debug;

    if (debug) {
        Serial.println(RESP_DBG_ON);
    } else {
        Serial.println(RESP_DBG_OFF);
    }

    // Terminate our serial response.
    Serial.println(RESP_OK);
}

// debug_print()
//
// Prints debug message to serial port if debugging output is enabled.
//
// Parameters:
//   msg - The message to print.  A newline will be added automatically.
//
void debug_print(String msg)
{
    if (debug) {
        Serial.print(LOG_DBG_PREFIX);
        Serial.println(msg);
    }
}

// usage()
//
// Prints usage details to serial port
//
void usage()
{
    Serial.println("--- Smart Card Removinator (v1.0.1) ---");
    Serial.println("Command Format:");
    Serial.print("\t");
    Serial.print("Prefix:");
    Serial.print("\t\t");
    Serial.println("#");
    Serial.print("\t");
    Serial.print("Terminator:");
    Serial.print("\t");
    Serial.println("\\r");
    Serial.println("Commands:");
    Serial.print("\t");
    Serial.print(CMD_CARD_N);
    Serial.print("N");
    Serial.print("\t");
    Serial.println("Insert card N (where N is 1-8)");
    Serial.print("\t");
    Serial.print(CMD_REMOVE_CARD);
    Serial.print("\t");
    Serial.println("Remove inserted card");
    Serial.print("\t");
    Serial.print(CMD_LOCK_N);
    Serial.print("N");
    Serial.print("\t");
    Serial.println("Lock card slot N (where N is 1-8)");
    Serial.print("\t");
    Serial.print(CMD_UNLOCK_N);
    Serial.print("N");
    Serial.print("\t");
    Serial.println("Unlock card slot N (where N is 1-8)");
    Serial.print("\t");
    Serial.print(CMD_STATUS);
    Serial.print("\t");
    Serial.println("Return card socket status");
    Serial.print("\t");
    Serial.print(CMD_DEBUG);
    Serial.print("\t");
    Serial.println("Toggle debugging output");
    Serial.print("\t");
    Serial.print(CMD_HELP);
    Serial.print("\t");
    Serial.println("Show usage information");

    // Terminate our serial response.
    Serial.println(RESP_OK);
}
