#!/bin/bash

OUTPUT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]%.*}").output"
KNOWN_HOST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/known_host"
SSH_LOGIN_STATEMENT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/ssh_login_statement"
OPTIONS=$(getopt -l "password:,server_url:" -o "p:s:" -a -- "$@")
eval set -- "$OPTIONS"

while true; do
case $1 in
-p|--password) 
    shift
    export PASSWORD=$1
    ;;
-s|--server_url) 
    shift
    export SERVER_URL=$1
    ;;
--)
    shift
    break;;
esac
shift
done

if [ $UID -eq 0 ]; then
    exec > "${OUTPUT}"
    which upterm > /dev/null || id upterm > /dev/null 2>&1 || { echo "Not installed"; exit 1; }
    chown upterm:sudo "${OUTPUT}"
    echo -e "${PASSWORD:=password}\n${PASSWORD:=password}" | passwd -q upterm 2>&1
    rm -f ${KNOWN_HOST} || true 
    touch ${KNOWN_HOST}
    chown upterm:sudo "${KNOWN_HOST}"
    rm -f ${SSH_LOGIN_STATEMENT} || true
    touch ${SSH_LOGIN_STATEMENT}
    chown upterm:sudo "${SSH_LOGIN_STATEMENT}"
    exec su -c "$0" upterm "$@"
    exit $?
fi

exec >> "${OUTPUT}"

# only upterm user can run
if [ $(whoami) != "upterm" ]; then
   echo "Permission deny"; exit 1;
fi

test -f ~/.ssh/id_rsa || ssh-keygen -q -t rsa -N '' -f ~/.ssh/id_rsa <<<y 2>&1 >/dev/null

PLUGIN_NAME=plugin_upterm
tmux kill-session -t $PLUGIN_NAME 2>&1 > /dev/null || true
tmux new-session -d -s $PLUGIN_NAME -n $PLUGIN_NAME 
tmux send-keys -t $PLUGIN_NAME:$PLUGIN_NAME "upterm host --known-hosts ${KNOWN_HOST} --server ${SERVER_URL}" Enter
sleep 2
tmux send-keys -t $PLUGIN_NAME:$PLUGIN_NAME "yes" Enter
sleep 3
tmux capture-pane -t $PLUGIN_NAME:$PLUGIN_NAME -S - -E - -p -J | grep "SSH Session:" | cut -d " " -f 3- | xargs > ${SSH_LOGIN_STATEMENT}
tmux send-keys -t $PLUGIN_NAME:$PLUGIN_NAME "q" Enter