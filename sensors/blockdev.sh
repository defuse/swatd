#!/bin/bash
readonly DEVICE=/dev/sdc

test -b "$DEVICE"
exit $?

