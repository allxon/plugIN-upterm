#!/bin/bash

plugin_appguid=9cdfc058-e3f8-4bf4-8621-d2114eade9d9

echo "Stop running plugIN-upterm."
currentShDirectory=$(dirname ${BASH_SOURCE})
sudo $currentShDirectory/scripts/stopPlugin.sh
sudo rm -rf $currentShDirectory