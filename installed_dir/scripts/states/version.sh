#!/bin/bash

OUTPUT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]%.*}").output"

exec > "${OUTPUT}"

which upterm > /dev/null || { echo "Not installed"; exit 0; }
upterm version 