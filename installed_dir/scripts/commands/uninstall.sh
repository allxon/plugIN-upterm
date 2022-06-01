#!/bin/bash

exec > "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]%.*}").output"

which upterm > /dev/null || { echo "Not installed"; exit 1; }

"$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/stop.sh"

killall -u upterm
rm -f /usr/bin/upterm
userdel -r upterm 2>&1
