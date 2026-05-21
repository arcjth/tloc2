#!/bin/bash
SKETCH="firmware.ino"
BOARD="esp32:esp32:esp32"
PORT="/dev/ttyUSB0"

arduino-cli compile --fqbn $BOARD $SKETCH && \
arduino-cli upload  --fqbn $BOARD --port $PORT $SKETCH && \
stty -F $PORT 115200 raw && \
cat $PORT