#!/bin/bash
readonly HOSTNAME=localhost
readonly PORT=80
readonly TIMEOUT=2

for i in {1..5}; do 
    if nc -w 2 -z "$HOSTNAME" "$PORT" > /dev/null 2>&1; then
        exit 0
    fi
done

exit 1
