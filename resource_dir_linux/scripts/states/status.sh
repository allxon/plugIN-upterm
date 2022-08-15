#!/bin/bash

OUTPUT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]%.*}").output"

exec > "${OUTPUT}" 

pidof upterm > /dev/null || { echo "Closed"; exit 0; }
echo "Open"