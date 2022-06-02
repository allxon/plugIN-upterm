#!/bin/bash

$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/commands/stop.sh
PLUGIN_PID=$(pgrep plugIN-upterm)
test $? -eq 0 || exit 0
kill -9 $PLUGIN_PID

while kill -0 $PLUGIN_PID > /dev/null 2>&1; do 
    sleep 1
done
