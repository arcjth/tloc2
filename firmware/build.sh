#!/bin/bash
SKETCH="firmware.ino"
BOARD="arduino:avr:nano:cpu=atmega328old"
PORT="/dev/ttyUSB0"

arduino-cli compile --fqbn $BOARD $SKETCH
arduino-cli upload  --fqbn $BOARD --port $PORT $SKETCH

sleep 1000

stty -F /dev/ttyUSB0 115200 raw
cat /dev/ttyUSB0
