#!/bin/bash

echo "Stop running plugIN-upterm."
currentShDirectory=$(dirname ${BASH_SOURCE})
$currentShDirectory/scripts/commands/uninstall.sh
$currentShDirectory/scripts/stopPlugin.sh
rm -rf $currentShDirectory