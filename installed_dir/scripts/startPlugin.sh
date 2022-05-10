#!/bin/bash

pgrep plugIN-upterm > /dev/null

if [[ $? -ne 0 ]]; then
	$(cd "$(dirname "${BASH_SOURCE[0]}")/../" && pwd)/plugIN-upterm $(cd "$(dirname "${BASH_SOURCE[0]}")/../" && pwd)/config/plugin_config_upterm.json > /dev/null &
fi
