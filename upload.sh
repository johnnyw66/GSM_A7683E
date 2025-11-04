#!/bin/bash
BOARD_FQBN="rp2040:rp2040:rpipico2w"
# Note PORT is usually something like /dev/tty.usbnodem101 - but I have a softlink
# (~/picodev) to this in my home directory
PORT="$HOME/picodev"
SKETCH="$PWD"

arduino-cli compile --fqbn $BOARD_FQBN $SKETCH --output-dir ./build
python3 $PWD/_reset.py
echo "Copying file to Volume..."
sleep 3
cp $SKETCH/build/*.uf2 /Volumes/RP2350/.
echo "Complete - Board should now be resetting"


