#!/bin/bash
SKETCH="main/main.ino"
BOARD="arduino:avr:nano:cpu=atmega328old"
PORT="/dev/ttyUSB1"

arduino-cli compile --fqbn $BOARD $SKETCH
arduino-cli upload  --fqbn $BOARD --port $PORT $SKETCH

sleep 1000

stty -F /dev/ttyUSB0 115200 raw
cat /dev/ttyUSB0
