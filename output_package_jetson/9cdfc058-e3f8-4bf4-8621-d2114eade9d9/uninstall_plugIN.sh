#!/bin/bash
plugin_appguid=9cdfc058-e3f8-4bf4-8621-d2114eade9d9
plugin_service=plugIN-upterm.service

disable_plugin_service() {
    if [ -f "/etc/systemd/system/${plugin_service}" ]; then
        echo "Find the service ${plugin_service} from system, disable the service first."
        systemctl stop ${plugin_service}
        systemctl disable ${plugin_service}  
        rm /etc/systemd/system/${plugin_service} 
    fi
}

remove_plugin() {
    sudo rm -rf /opt/allxon/plugIN/$plugin_appguid
}

uninstall_plugIN() {

    disable_plugin_service

    remove_plugin

    exit 0
}

uninstall_plugIN