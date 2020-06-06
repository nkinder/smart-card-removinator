# Copyright 2016 Nathan Kinder and the Smart Card Removinator contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""This module provides a convenient way of interfacing with the serial
interface of the Smart Card Removinator."""

import io
import json
import os
import subprocess

import serial


def _discover_removinator():
    """Attempt to discover the serial port for a Removinator controller."""

    usb_dev_root = '/sys/bus/usb/devices/'
    usb_dev_dirs = (path for path in os.listdir(usb_dev_root)
                    if path.startswith('usb'))

    for path in usb_dev_dirs:
        for root, dirs, files in os.walk('{0}{1}'.format(usb_dev_root, path)):
            if ('dev' in files):
                device = subprocess.check_output(['udevadm', 'info', '-q',
                                                  'name', '-p', '{0}'
                                                  .format(root)],
                                                 encoding='utf-8').rstrip()
                if (device.startswith('tty')):
                    output = subprocess.check_output(['udevadm', 'info', '-q',
                                                      'property', '--export',
                                                      '-p', '{0}'.format(root)],
                                                     encoding='utf-8')
                    try:
                        output.index('ID_MODEL=\'Arduino_Micro\'')
                        return '/dev/{0}'.format(device)
                    except ValueError:
                        pass

    raise ConnectError('Unable to discover Removinator controller')


# Custom Exceptions
class Error(Exception):
    """Base class for exceptions in this module.

    :param msg: Error message associated with the exception
    :type msg: str
    :ivar msg: Error message associated with the exception
    """

    def __init__(self, msg):
        self.msg = msg

    def __str__(self):
        return self.msg


class CommandError(Error):
    """Exception raised sending a command to the Removinator."""
    pass


class ConnectError(Error):
    """Exception raised connecting to the Removinator controller."""
    pass


class SlotError(Error):
    """Exception raised selecting an empty card slot."""
    pass


class Removinator:
    """Connection to a Smart Card Removinator controller"""

    def __init__(self, port=None):
        """Opens a connection to a Removinator controller on the specified
        serial port.

        If a port is not specified, an attempt will be made to auto-discover
        the Removinator controller.

        :param port: port that the Removinator is connected to
        :type port: str
        :raises: :exc:`ConnectError`
        :ivar last_result: The result output from the last executed command
        :ivar last_response: The response output from the last executed command
        :ivar port: The port that the Removinator is connected to
        """

        self.port = port
        self.last_result = ''
        self.last_response = ''

        # Attempt to discover the Removinator controller if a
        # port was not specified.
        if port is None:
            port = _discover_removinator()

        # Open a connection to the Removinator controller.
        try:
            self.connection = serial.Serial(
                port,
                9600,
                serial.EIGHTBITS,
                serial.PARITY_NONE,
                serial.STOPBITS_ONE,
                1)
            self.sio = io.TextIOWrapper(io.BufferedRWPair(self.connection,
                                                          self.connection))
        except serial.SerialException as e:
            raise ConnectError('Unable to open connection to Removinator '
                               'controller on port {0}'.format(port))

    def insert_card(self, slot):
        """
        Insert a specified card

        This sends a command to the Removinator conroller telling it to insert
        a specified card to the card reader.  If the specified slot does not
        have a card present, this will raise a *SlotError*.

        :param slot: the slot of the card to insert (1-8)
        :type slot: int
        :raises: :exc:`SlotError`, :exc:`CommandError`, :exc:`ValueError`
        """

        if (1 <= slot <= 8):
            try:
                self.send_command('SC{0}'.format(slot))
            except CommandError as e:
                # Raise a specific exception if an empty slot was selected.
                if (self.last_result == 'ERR_NOCARD'):
                    raise SlotError('No card present in slot {0}'.format(slot))

                # Re-raise the original exception for all other errors.
                raise
        else:
            raise ValueError('Slot must be between 1 and 8')

    def remove_card(self):
        """
        Remove the currently inserted card

        :raises: :exc:`CommandError`
        """

        self.send_command('REM')

    def get_status(self):
        """
        Get the status of the Removinator controller

        The status indicates the currently inserted card as well as a list of
        slots that have a card present.

        :return: status containing the current and present cards
        :rtype: dict containing *current* and *present* keys
        :raises: :exc:`CommandError`
        """

        self.send_command('STA')

        # Strip out any debug output responses before
        # attempting to parse the JSON status.
        for resp_line in self.last_response.splitlines():
            if (not resp_line.startswith('[DBG]')):
                break

        return json.loads(resp_line)

    def send_command(self, command):
        """
        Send a command to the Removinator controller

        Sends the specified command to the Removinator controller.  The command
        result (*OK* or *ERR_\\**) will be available in the *last_result*
        attribute of your *Removinator* object.  Similarly, the full command
        response will be available in the *last_response* attribute.

        If an error result is encountered, a CommandError will be
        raised.

        :param command: the command to send
        :type command: str
        :raises: :exc:`CommandError`
        """

        # Clear out the previous command result and response.
        self.last_result = ''
        self.last_response = ''

        # Send the command in a proper envelope to the Removinator controller.
        try:
            self.connection.write('#{0}\r'.format(command).encode())
        except serial.SerialException as e:
            raise CommandError('Serial error encountered sending command '
                               '\"{0}\" ({1})'.format(command, e))

        # Attempt to read the response via serial.
        self.connection.reset_input_buffer()
        for resp_line in self.sio:
            # Check if we've hit the final result line of the response.
            if (resp_line.startswith('OK')):
                self.last_result = resp_line.rstrip()
                break
            elif (resp_line.startswith('ERR_')):
                self.last_result = resp_line.rstrip()
                raise CommandError('{0} encountered sending command \"{1}\"'
                                   .format(self.last_result, command))

            # Concatenate multi-line responses.
            self.last_response += resp_line

    def set_debug(self, debug):
        """
        Enable or disable Removinator controller debug output

        When enabled, the Removinator controller will return verbose responses
        via serial when commands are executed.  The debug output from the
        previously run command will be available along with the standard
        command output in the *last_response* attribute of your
        *Removinator* object.

        :param debug: enable/disable debugging
        :type debug: bool
        :raises: :exc:`CommandError`
        """
        # Send the debug command.
        self.send_command('DBG')

        # Check the response to see if we are in the requested state.
        if (debug and (self.last_response.rstrip().endswith('DBG_OFF'))):
            self.send_command('DBG')
        elif (not debug and (self.last_response.rstrip().endswith('DBG_ON'))):
            self.send_command('DBG')
