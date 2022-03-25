#!/bin/bash

plugin_appguid=e5c7f65b-1cf7-46e6-af56-d418ac43acc2

echo "Stop running plugIN-upterm."
currentShDirectory=$(dirname ${BASH_SOURCE})
$currentShDirectory/scripts/commands/uninstall.sh
$currentShDirectory/scripts/stopPlugin.sh
rm -rf $currentShDirectory