# upterm plugIN

upterm plugIN is a plugIN to enable upterm web console remotely with allxon DMS. It connects to the allxon DMS Agent running on a device which you want to control remotely.
This project contains source code to build the upterm plugIN executive and script files which are needed when running the plugIN to control upterm application. It includes the following files and folders.

Source Code
- MainScr - main function, websocketclient and states control of websocket connections and Agent / Device plugIN API
- Util - utilities, header files of allxon DMS plugIN SDK library, cJSON library, header files of argon2 library
- Plugins - classes and functions specific related to upterm plugIN.
- websocket/websocketpp - Websocket++, a C++ websocket client/server library
- lib - allxon DMS plugIN SDK library and argon2 library
- Makefile -  used to build, install, uninstall the target, and package it to upload to the allxon DMS server for destributing to managed devices.

Scripts and Configurations
- config - configuration files for setting this plugIN and what this plugIN looks like on the allxon DMS portal.
- scripts - scripts for called when running commands by users remotely via portal and update states for the current status of upterm app. Otherwise, scripts for start and stop the plugIN itself remotely.
- install - scripts for install and uninstall this plugIN when it is destributed from portal.

## Dependency Packages

- boost
 ```bash
 $ sudo apt-get install libboost-all-dev
 ```
- OpenSSL
 ```bash
 $ sudo apt-get install libssl-dev
 $ sudo apt-get install libcrypto
 ```
- rt, pthread

## Build this project

After installed all dependency packages, you can build this project under the folder contains Makefile.
- build code when there're updates
 ```bash
 $ make 
 ```

- clean all files generated by build
 ```bash
 $ make clean 
 ```

- rebuild the project
 ```bash
 $ make rebuild 
 ```

- put upterm plugIN binary and related files to the specific path to work properly. You need to generate an executive beforehand.
 ```bash
 $ make install
 ```
  Or do make and copy files together
 ```bash
 $ make default install
 ```

- uninstall upterm plugIN on this device.
 ```bash
 $ make uninstall
 ```

- package needed fiies to a zip file for registering a new plugIN or update to new versions. You need to generate an executive beforehand.
 ```bash
 $ make package
 ```

## Modify source code

- **Log file path** - You can modify the log output path in /MainSrc/Log.cpp. Currently the log files will be stored under **"/var/log/allxon/plugIN/[appGUID]/[appName]/logs/[TitleFileName]/[TitleFileName].[YYYYMMDD_hhmmss_xxxxxx].log**

- **Customize your own upterm plugIN** - Modify **/Plugins/uptermPlugin.cpp** and its header file to add or modify the **CuptermPlugin** class. This class is derived from _CPluginSample_ class which keeps basic configurations of this plugin, including reading the **plugin_config_xxx.json** properties, i.e., if sending API message as minified json?, the Agent plugIN API version (only support v2 currently), the appGUID and its accessKey, and reading the **xxxPluginUpdate.json** which is the registration API of this plugIN for later use. You can add methods used specifically for this plugIN in the _CuptermPlugin_ class, or create your own plugIN class derived from CPluginSample class.

- **Update the allxon plugIN SDK to a new version** - put the header files to _/Util/include/_ folder and its static or shared lib to _/lib/_ folder. Or you can put them to the place where you assigned in the Makefile.

- _states_ will be updated once the plugIN is running and before it was stopped, and every 60 seconds during the plugIN is alive. You can modify this behavior in the static function **_NotifyDataThread()_** in _/MainSrc/WebSocketClient.cpp_

- _command_ ack will be sent after received a command and when the command executed with results message regarding its result status. You can modify the results or ack behaviors in CuptermPlugin::ExecuteReceivedCommand method.

## What does Scripts do?

- install_plugIN.sh - To move the upterm_plugIN executive and related script files in the package to specific location. Then run startPlugin.sh to launch upterm_plugIN.

- uninstall_plugIN.sh - First to stop running upterm_plugIN on the device. Then to remove upterm plugIN related files from the device. This script will be called when you remove the upterm plugIN via allxon DMS portal.

- startPlugin.sh - To launch upterm plugIN on the device. Set the full path of this file in the xxxPluginUpdate.json to "startCommand" property to enable starting the plugIN on the portal. The "Start" button will be shown on the title bar of plugIN card of this plugIN.

- stopPlugin.sh - To stop running the upterm plugIN on the device. Set the full path of this file in the xxxPluginUpdate.json to "stopCommand" property to enable stopping the plugIN on the portal. The "Stop" button will be shown on the title bar of plugIN card of this plugIN.

- commands
    * install.sh - To install upterm application on this device.
    * uninstall.sh - To uninstall upterm application on this device.
    * start.sh - To open a remote terminal instant sharing session.
    * stop.sh - To close the opened session.

- states
    * version.sh - To get the upterm software version number
    * status.sh - To get the upterm current status, i.e. Not running, Open and Closed.
    * ssh.sh - To get the opened session link for ssh.
    * web.sh - To get the opened session link for web remote terminal.

## Developer Zone
```
# build 
$ sudo docker build --build-arg ARCH=<x86|jetson> .

# export binary to `output` folder
$ sudo docker build -o output --build-arg ARCH=<x86|jetson> .
```
