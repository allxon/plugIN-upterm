#!/bin/bash

OUTPUT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]%.*}").output"

if [ $UID -eq 0 ]; then
    exec > "${OUTPUT}"
    which upterm > /dev/null || id upterm > /dev/null 2>&1 || { echo "Not installed"; exit 0; }
    chown upterm:sudo "${OUTPUT}"
    exec su -c "$0" upterm "$@"
    exit $?
fi

exec >> "${OUTPUT}"

# only upterm user can run
if [ $(whoami) != "upterm" ]; then
   echo "Permission deny"; exit 0;
fi

which upterm > /dev/null || { echo "Not installed"; exit 0; }
which tmux > /dev/null || { echo "Not installed"; exit 0; }
tmux kill-session -t plugin_upterm:plugin_upterm > /dev/null && echo "Stopped"