#!/bin/bash

CURRENT_SH_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
OUTPUT="${CURRENT_SH_DIR}/$(basename "${BASH_SOURCE[0]%.*}").output"
CURRENT="${CURRENT_SH_DIR}/$(basename "${BASH_SOURCE[0]%.*}").current"

exec > "${CURRENT}"

while :
do
	# pidof upterm > /dev/null || { echo "{\"url\":\"\",\"alias\":\"N/A\"}"; break; }
	# URL=$(cat ${CURRENT_SH_DIR}/../commands/ssh_login_statement)
	# WEB_URL="${URL/ssh /"ssh://"}"
	# WEB_URL="${WEB_URL/ -p /":"}"
	# echo "{\"url\":\"${WEB_URL}\",\"alias\":\"${URL}\"}"

	pidof upterm > /dev/null || { echo "N/A"; break; }
	URL=$(cat ${CURRENT_SH_DIR}/../commands/ssh_login_statement)
	echo ${URL}
	break
done

if ! test -f ${OUTPUT} > /dev/null || ! diff ${OUTPUT} ${CURRENT} > /dev/null; then
        mv ${CURRENT} ${OUTPUT}
fi
