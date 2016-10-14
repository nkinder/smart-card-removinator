Smart Card Removinator
======================

The Smart Card Removinator is a hardware device designed for automation of
smart card testing with physical tokens.  Testing with physical tokens often
requires smart cards to by removed and inserted from the card reader to triger
things like screen locking, or simply to test multiple models of tokens or with
different certificates.  This type of testing is usually handled manually when
testing with emulated tokens is not desirable.

The Smart Card Removinator is essentially a smart card switch that is
controlled via a RS-232 serial interface.  It allows for 8 smart cards to be
switched to a single output that is connected to a card reader.  This allows
token insertion and removal events to be automated for multiple tokens.

Hardware
--------

The hardware for the Smart Card Removinator is composed of two components:
a controller and a reader adapter.

### Controller
The controller is based on an Arduino (Atmel ATmega32U4) microcontroller.  The
microcontroller provides the USB interface that is used to interface with the
computer that drives the controller.  The microcontroller also handles
communication with the various ICs that are used in the controller.

The heart of the controller is the switching circuit that is used to actually
switch between the pyhsical smart cards.  Switching is handled by a set of
analog multiplexer ICs are used to switch the contacts of the 8 controller
card slots to a single output that connects to the card reader contacts via a
card adapter.  One 8:1 multiplexer is used for each contact of the smard card,
allowing for 8 total cards that can be switched.  The logic control pins of the
multiplexer ICs are tied together so they are controlled consistently from the
same signals.  The controller uses Maxim MAX4638 multiplexer ICs, whose
switching behavior is controlled by the microcontroller via 4 digital logic
pins.

The card slots/connectors used on the controller feature a switch that is used
to detect if a card is present or not.  The firmware uses these for error
checking (selecting a card slot that is not populated) and for status
reporting.  The controller uses a parallel-in, serial-out (PISO) shift register
that the microcontroller interfaces with via Serial Peripheral Interface (SPI).
This allows us to simultaneously read the state of all 8 card slots while using
a minimum number of control pins.

The controller features a 7-segment display that is used to indicate the
currently selected card slot.  The display is controlled via a serial-in,
parallel-out (SIPO) shift register that the microcontroller interfaces with via
Serial Peripheral Interface (SPI).  This allows us to simultaneously turn on
the required display segments while using a minimum number of control pins.

#### Schematics
Electrical schematics for the controller are available in the _schematics_
directory.  They are provided in SVG format.  The controller is spread across
3 schematics to keep the images at a reasonable size for printing.  Labels are
used to show where connections are made between components on different
schematics (or even within a single schematic to simplify things visually).

All of the schematics are also displayed below for convenience.  You can click
on the images to display a larger version.

The following schematic contains the microcontroller connector, the display
circuit, and the card socket presence circuit.

[![Controller main](https://nkinder.github.io/images/smart-card-removinator/controller-main.svg "Controller main")][controller-main]

The following schematic contains the multiplexor circuit and the smart card
reader adapter socket.

[![Controller multiplexer](https://nkinder.github.io/images/smart-card-removinator/controller-multiplexer.svg "Controller multiplexer")][controller-mux]

The following schematic contains the smart card sockets as well as the A-to-B
board connectors (described in more detail in the PCBs section below).  This
schematic is really only useful for understanding the pinout of these
connectors.

[![Controller sockets](https://nkinder.github.io/images/smart-card-removinator/controller-sockets.svg "Controller sockets")][controller-sockets]

#### PCBs
The controller is made up of 2 PCBs, which stack on top of each other.  They
are connected via 2 24-pin header connectors.  The bottom board is referred to
as the A board, and the top board as the B board.  It is designed this way to
keep the PCB footprint from getting too large, as the physical size of the
smart card slots would cause a single board design to be fairly long depending
on the layout.  Each board has 4 smart card slots arranged across the front
edge of the board.

Board A contains the following components/circuits:

  * Microcontroller (USB port)
  * Reader adapter RJ45 port
  * Multiplexer circuits
  * Card presence circuits
  * Card slots 5-8

Board B contains the following components/circuits:

  * Display circuits and display
  * Card slots 1-4

TODO(NGK) Add more details once the PCB design is added to the repo

### Reader Adapter
The reader adapter is a simple PCB that has contact pads in the standard
location described in ISO 7816, which are connected to a RJ45 jack via traces.
It is important that the reader adapter uses a .031" thick PCB, otherwise it
may not fit into the slot of your smart card reader.

TODO(NGK) Add more details once the PCB design is completed.

### Component List
The list of required components for the controller and reader adapter are
listed below.  Substitutions can be easily made for the common components such
as resistors, capacitors, IC sockets, and A-to-B board connectors.
Substitutions for other components may require firmware or PCB design changes,
so it is recommended that you consult the component datasheets to see if
changes will be required.

* (1) Arduino Micro 
* (8) Maxim Integrated MAX4638ESE+
  * Analog Multiplexers (SOIC-16)
* (1) Texas Instruments SN74HC595N
  * Display shift register (DIP-16)
* (1) Texas Instruments SN74HC165N
  * Smart card presense shift register (DIP-16)
* (1) Kingbright SA56-51PBWA/A
  * 7-segment display
* (8) Amphenol C702 10M008 230 40
  * Smart card slots
* (2) Amphenol RJE7318800310
  * RJ45 sockets for controller and reader adapter
* (10) Vishay K104K15X7RF5TL2
  * 0.1uF decoupling capacitors for ICs
* (10) KOA Speer MF1/4DCT52R3300F
  * 330ohm resistors to protect LEDs (display, RJ45 LEDs)
* (8) KOA Speer MF1/4DCT52R1002F
  * 10Kohms pull-up resistors for card presence circuits
* (4) KOA Speer MF1/4DCT52R1001F
  * 1Kohms resistors for digital output pin protection
* (2) 3M 4816-3000-CP
  * DIP-16 sockets for shift registers
* (2) TE Connectivity 6-102619-0
  * Lower (A) board connectors
* (2) TE Connectivity 6-534206-2
  * Upper (B) board connectors
* (2) TE Connectivity 6-535541-6
  * Microcontroller header sockets

Firmware
--------

The firmware for the microcontroller implements a RS-232 command protocol that
can be used over the USB interface to control the controller.  A command must
be sent in an envelope, which means that it must be prefixed with a '#'
character and terminated with a carriage return ('\r').  The current list of
supported commands (as provided by the usage command output) is as follows:

```
--- Smart Card Removinator (v0.1) ---
Command Format:
	Prefix:		#
	Terminator:	\r
Commands:
	SC1	Insert card 1
	SC2	Insert card 2
	SC3	Insert card 3
	SC4	Insert card 4
	SC5	Insert card 5
	SC6	Insert card 6
	SC7	Insert card 7
	SC8	Insert card 8
	REM	Remove inserted card
	STA	Return card socket status
	DBG	Toggle debugging output
	?	Show usage information
```

The command responses are terse to limit the amount of data sent over the
serial port, but they provide enough detail to indicate success or uniquely
identify common errors.

If an illegal command is entered (missing envelope, unknown command, etc.), the
following response codes can be returned:

  * _ERR_UNKOWN_CMD_
    * A propoer envelope was used, but the command is unknown.
  * _ERR_CMDLEN_
    * The command envelope prefix was used, but we didn't receive a terminator.
  * _ERR_PROTOCOL_
    * Input was received with no envelope prefix.

The `SC*` commands can reutrn the following responses:

  * _OK_
    * The card was successfully inserted/switched.
  * _ERR_NOCARD_
    * No card is present in the specified slot.

The `REM` command will always return a response of 'OK'.  Using this command
when no card is currently selected is a no-op and is not considered an error.

The `DBG` command toggles debugging output on or off.  It can return the
following responses:

  * _DBG_ON_
    * Debug output is now enabled.
  * _DBG_OFF_
    * Debug output is now disabled.

Note that if debug output is enabled, debugging messages will be returned as a
part of the responses for the above commands in addition to the normal
responses.  Debug output responses will always be prefied by `[DBG]`, as can be
seen in the following example response to the `SC1` command:

```
[DBG] Received command "SC1"
[DBG] Received status: 0
ERR_NOCARD
```

The `?` command responds with the usage information that is shown above.  It is
intended for interactive use of the controller via a terminal.

[controller-main]: https://nkinder.github.io/images/smart-card-removinator/controller-main.svg
[controller-mux]: https://nkinder.github.io/images/smart-card-removinator/controller-multiplexer.svg
[controller-sockets]: https://nkinder.github.io/images/smart-card-removinator/controller-sockets.svg
