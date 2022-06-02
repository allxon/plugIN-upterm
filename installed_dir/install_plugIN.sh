#!/bin/bash

PLUGIN_NAME=plugIN-upterm
START_PLUGIN_RELATIVE_PATH=scripts/startPlugin.sh

CURRENT_SH_DIRECTORY=$(cd "$(dirname "${BASH_SOURCE[0]}")/" && pwd)
PLUGIN_APP_GUID=${ALLXON_PLUGIN_DIR##*/}

if [ -d $ALLXON_PLUGIN_DIR ]; then
   echo "ERROR: plugin $PLUGIN_APP_GUID already installed"
   exit 1
else 
   mkdir -p $ALLXON_PLUGIN_DIR
fi 

output_file="install_plugIN_$PLUGIN_APP_GUID.output"

EXECUTABLE_DESCRIPTION=$(file $CURRENT_SH_DIRECTORY/$PLUGIN_APP_GUID/$PLUGIN_NAME)
ARCH=$(uname -i)

if [[ "$ARCH" == "x86_64" ]]; then
   ARCH="x86-64"
fi
if [[ "$EXECUTABLE_DESCRIPTION" != *"$ARCH"* ]]; then
   echo "Not Supported Architecture" > $output_file 2>&1
   exit 1
fi

cp -r $CURRENT_SH_DIRECTORY/$PLUGIN_APP_GUID/* $ALLXON_PLUGIN_DIR

echo "plugIN is installed to $ALLXON_PLUGIN_DIR/"

$ALLXON_PLUGIN_DIR/$START_PLUGIN_RELATIVE_PATH
