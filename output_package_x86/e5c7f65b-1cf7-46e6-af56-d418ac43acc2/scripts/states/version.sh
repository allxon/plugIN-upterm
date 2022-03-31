#!/bin/bash

OUTPUT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]%.*}").output"
CURRENT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]%.*}").current"

exec > "${CURRENT}"

while :
do
	which upterm > /dev/null || { echo "Not installed"; break; }

	upterm version 

	break
done

if ! test -f ${OUTPUT} > /dev/null || ! diff ${OUTPUT} ${CURRENT} > /dev/null; then
        mv -f ${CURRENT} ${OUTPUT}
fi