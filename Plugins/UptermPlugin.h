#ifndef _UPTERMPLUGIN_H_
#define _UPTERMPLUGIN_H_

#include <bitset>
#include "PluginSample.h"
#include "AlertsPluginJson.h"

class cJSON;

#define SAMPLE_UPTERM_PLUGIN      "UptermPlugin"
#define PLUGINS_PATH                "/opt/allxon/plugIN/"
#define SCRIPTS_STATES_PATH         "scripts/states/"
#define SCRIPTS_COMMANDS_PATH       "scripts/commands/"
#define CONFING_PATH                "config/"
#define PLUGIN_UPDATE_CONFIG        "uptermPluginUpdate.json"


class StateUpdated
{
public:
    const static std::bitset<4> status;
    const static std::bitset<4> version;
    const static std::bitset<4> ssh;
    const static std::bitset<4> none;
};

class AlertStatus
{
public:
    const static std::bitset<1> alarm1;
    const static std::bitset<1> none;

    static std::string GetName(std::bitset<1> alertStatus);
};

class UptermStates
{
public:
    const static std::string status;
    const static std::string version;
    const static std::string ssh;
};

class UptermCommands
{
public:
    const static std::string install;
    const static std::string start;
    const static std::string stop;
    const static std::string uninstall;
    const static std::string password;
    const static std::string server_url;
};

class CUptermPlugin : public CPluginSample
{
public:
    CUptermPlugin();
    CUptermPlugin(CPluginSampleConfig *sampleConfig);
    CUptermPlugin(std::string apiVersion, bool minify, std::string appGUID, std::string accessKey);
    ~CUptermPlugin();

    CUpdatePluginJson *SetNotifyPluginUpdate();
    CAlarmUpdatePluginJson *GetAlarmUpdateObject() { return m_alarmUpdateObj; }
    bool AcceptReceivedCommand(std::string cmdName, std::map<std::string, std::string> params, std::string& reason);
    std::string ExecuteReceivedCommand(std::string cmdName, std::map<std::string, std::string> params, cJSON *cmdAck);
    void UpdateStates(std::bitset<4> updateMask);
    cJSON *AddStatusState();
    cJSON *AddVersionState();
    cJSON *AddSshLinkState();
    std::bitset<4> IsStateFilesChanged();
    void UpdateAlarmsData(const char *payload);

    std::string m_pluginPath;
    std::string m_statusOutput;
    std::string m_versionOutput;
    std::string m_sshOutput;
    const static std::string moduleUpterm;

protected:
    void Init();
    static std::string ReadOutput(const std::string outputName);
    static bool RunStatesScript(std::string scriptCmd);
    static cJSON *GetStatesOutput(std::string pluginPath, std::string stateName);
    static void RunPluginScriptCmd(const std::string scriptCmd, const std::string cmdParam, bool &result);
    static void RunPluginScriptCmdOutput(const std::string scriptCmd, const std::string cmdParam, bool &result, std::string &output);
    static std::string RandomString(int len);
    static int FileIsModified(const char *path, time_t oldMTime, time_t &newMTime);

private:
    time_t statusTime;
    time_t versionTime;
    time_t sshTime;
    std::bitset<1> m_alertsStatus;
    CAlarmUpdatePluginJson *m_alarmUpdateObj;
};

#endif
