#!/bin/bash

CURRENT_SH_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
OUTPUT="${CURRENT_SH_DIR}/$(basename "${BASH_SOURCE[0]%.*}").output"

exec > "${OUTPUT}"

pidof upterm > /dev/null || { echo "N/A"; exit 0;}
URL=$(cat ${CURRENT_SH_DIR}/../commands/ssh_login_statement)
echo "${URL}"
