#!/bin/bash
DEVICE=$1

# Default device value
: ${DEVICE:="/dev/input/mouse0"}

test -c "$DEVICE"
exit $?

