#!/bin/bash
readonly HOSTNAME=localhost
readonly TIMEOUT=2

for i in {1..5}; do 
    if ping -c 1 -w "$TIMEOUT" "$HOSTNAME" > /dev/null 2>&1; then
        exit 0
    fi
done

exit 1
