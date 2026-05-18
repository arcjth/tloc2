#!/bin/bash
SKETCH="firmware.ino"
BOARD="arduino:avr:nano:cpu=atmega328"
PORT="/dev/ttyUSB0"

arduino-cli compile --fqbn $BOARD $SKETCH && \
arduino-cli upload  --fqbn $BOARD --port $PORT $SKETCH && \
stty -F $PORT 115200 raw && \
cat $PORT