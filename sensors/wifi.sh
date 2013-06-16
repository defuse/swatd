#!/bin/bash
readonly SSID="your-ssid"
readonly MAC_ADDRESS="00:00:00:00:00:00"

if iwconfig 2>&1 | fgrep -q "ESSID:\"$SSID\""; then
    if iwconfig 2>&1 | fgrep -q "$MAC_ADDRESS"; then
        exit 0
    else
        exit 1
    fi
else
    exit 1
fi

