#!/bin/bash

$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/commands/stop.sh
pgrep plugIN-upterm | xargs -i kill -2 {}
