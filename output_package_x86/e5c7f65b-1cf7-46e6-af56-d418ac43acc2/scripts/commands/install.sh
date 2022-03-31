#!/bin/bash
set -e
exec > "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]%.*}").output"

id upterm > /dev/null 2>&1 || useradd -m upterm -s /bin/bash -g sudo 
which apt-get > /dev/null || { echo "Not supported!"; exit 1; }
which tmux > /dev/null && tmux -V && which upterm > /dev/null && upterm version && exit 0;

# install tmux
apt-get update && apt-get install -y tmux

# install upterm
test -e /tmp/plugin-upterm.tar.gz || rm -rf /tmp/plugin-upterm.tar.gz
test -e /tmp/plugin-upterm || rm -rf /tmp/plugin-upterm

# Check architecture
arch=$(uname -i)
if [[ $arch == "x86_64" ]]; then
    url="https://github.com/owenthereal/upterm/releases/download/v0.7.5/upterm_linux_amd64.tar.gz"
elif [[ $arch == "aarch64" ]]; then
    url="https://github.com/owenthereal/upterm/releases/download/v0.7.6/upterm_linux_arm64.tar.gz"
else
    echo "unknown platform"
    exit 1
fi

wget -qO /tmp/plugin-upterm.tar.gz ${url}
mkdir /tmp/plugin-upterm && tar -xf /tmp/plugin-upterm.tar.gz -C /tmp/plugin-upterm
cp /tmp/plugin-upterm/upterm /usr/bin/

rm -rf /tmp/plugin-upterm.tar.gz
rm -rf /tmp/plugin-upterm