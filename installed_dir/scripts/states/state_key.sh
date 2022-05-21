#!/bin/bash

CURRENT_SH_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
OUTPUT="${CURRENT_SH_DIR}/$(basename "${BASH_SOURCE[0]%.*}").output"
CURRENT="${CURRENT_SH_DIR}/$(basename "${BASH_SOURCE[0]%.*}").current"

exec > "${CURRENT}"

date

if ! test -f ${OUTPUT} > /dev/null || ! diff ${OUTPUT} ${CURRENT} > /dev/null; then
        mv ${CURRENT} ${OUTPUT}
fi
