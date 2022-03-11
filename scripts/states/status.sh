#!/bin/bash

OUTPUT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]%.*}").output"
CURRENT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]%.*}").current"

exec > "${CURRENT}" 

while :
do
	pidof upterm > /dev/null || { echo "Closed"; break; }

	echo "Open"

	break
done

if ! test -f ${OUTPUT} > /dev/null || ! diff ${OUTPUT} ${CURRENT} > /dev/null; then
	mv -f ${CURRENT} ${OUTPUT}
fi
