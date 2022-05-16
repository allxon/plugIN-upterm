#!/bin/bash
plugin_appguid=e5c7f65b-1cf7-46e6-af56-d418ac43acc2
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