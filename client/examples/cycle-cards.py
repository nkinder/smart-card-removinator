#!/bin/python

import removinator
import subprocess

# This example cycles through each card slot in the Removinator.  Any
# slots that have a card present will then have the certificates on the
# card printed out using the pkcs15-tool utility, which is provided by
# the OpenSC project.
#
# Examples of parsing the Removinator status output and enabling debug
# output from the firmware are also provided.

print('--- Connecting to Removinator ---')
ctl = removinator.Removinator()

print('--- Cycling through cards ---')
for card in range(1, 9):
    try:
        ctl.insert_card(card)
        print('Inserted card {0}'.format(card))
        print('{0}'.format(subprocess.check_output(['pkcs15-tool',
                                                   '--list-certificates'])
                                                   .rstrip()))
    except removinator.SlotError:
        print('Card {0} is not inserted'.format(card))

print('--- Checking Removinator status ---')
status = ctl.get_status()
print('Current card: {0}'.format(status['current']))
for card in status['present']:
    print('Card {0} is present'.format(card))

print('--- Debug output for re-insertion of current card ---')
ctl.set_debug(True)
ctl.insert_card(status['current'])
print('{0}'.format(ctl.last_response.rstrip()))
ctl.set_debug(False)

print('--- Remove current card ---')
ctl.remove_card()
