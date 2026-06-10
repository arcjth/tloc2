#!/usr/bin/env bash
# TLOC2 — build/flash/monitor via ESP-IDF
#   nix-shell -p esp-idf

PORT="${1:-/dev/ttyUSB0}"   

BAUD=115200

set -e

idf.py build

idf.py -p "$PORT" -b "$BAUD" flash

# monitor usa Ctrl+] para sair
idf.py -p "$PORT" monitor

