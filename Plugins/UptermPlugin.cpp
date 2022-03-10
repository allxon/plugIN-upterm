#include "UptermPlugin.h"
#include "../Util/include/Utl_Log.h"
#include "../PluginSDK/PluginException.h"
#include <fstream>
#include <thread>

using namespace std;

#define SCRIPT_EXT          ".sh"
#define SCRIPT_OUTPUT_EXT   ".output"

const bitset<4> StateUpdated::status = bitset<4>("0001");
const bitset<4> StateUpdated::version = bitset<4>("0010");
const bitset<4> StateUpdated::web = bitset<4>("0100");
const bitset<4> StateUpdated::ssh = bitset<4>("1000");
const bitset<4> StateUpdated::none = bitset<4>("0000");
const bitset<1> AlertStatus::alarm1 = bitset<1>("1");
const bitset<1> AlertStatus::none = bitset<1>("0");
const string UptermStates::status = "status";
const string UptermStates::version = "version";
const string UptermStates::web = "web";
const string UptermStates::ssh = "ssh";
const string UptermCommands::install = "install";
const string UptermCommands::start = "start";
const string UptermCommands::stop = "stop";
const string UptermCommands::uninstall = "uninstall";
const string UptermCommands::password = "password";
const string CUptermPlugin::moduleUpterm = "uptermWebConsole";

string AlertStatus::GetName(bitset<1> alertStatus)
{
    if (alertStatus == AlertStatus::none) return "none";
    else
    {
        string alertStatusName;
        if ((alertStatus & AlertStatus::alarm1) == AlertStatus::alarm1) alertStatusName.append("alarm1|");
        return alertStatusName;
    }
}

CUptermPlugin::CUptermPlugin():CPluginSample()
{
    Init();
}

CUptermPlugin::CUptermPlugin(CPluginSampleConfig *sampleConfig):CPluginSample(sampleConfig)
{
    Init();
}

CUptermPlugin::CUptermPlugin(string apiVersion, bool minify, string appGUID, string accessKey)
    :CPluginSample(apiVersion, minify, appGUID, accessKey)
{
    Init();
}

CUptermPlugin::~CUptermPlugin()
{
}

CUpdatePluginJson *CUptermPlugin::SetNotifyPluginUpdate()
{
    string configFile = m_pluginPath;
    configFile.append(CONFING_PATH).append(PLUGIN_UPDATE_CONFIG);
    CUpdatePluginJson *updatePluginObj = SetNotifyPluginUpdateFromFile(configFile);

    return updatePluginObj;
}

bool CUptermPlugin::AcceptReceivedCommand(string cmdName, map<string, string> params, string& reason)
{
    bool result = true;
    if (!cmdName.empty())
    {
        if (cmdName.compare(UptermCommands::start) == 0 || cmdName.compare(UptermCommands::stop) == 0 ||
            cmdName.compare(UptermCommands::install) == 0 || cmdName.compare(UptermCommands::uninstall) == 0)
        {
            result = true;
            reason = "OK";
        }
    }
    else
    {
        result = false;
        reason = "Rejected.";
    }
    UTL_LOG_INFO("reason: %s", reason.c_str());
    return result;  
}

string CUptermPlugin::ExecuteReceivedCommand(string cmdName, map<string, string> params, cJSON *cmdAck)
{
    string cmdState = "";
    if (cmdName.compare(UptermCommands::start) == 0 || cmdName.compare(UptermCommands::stop) == 0 ||
        cmdName.compare(UptermCommands::install) == 0 || cmdName.compare(UptermCommands::uninstall) == 0)
    {
        string scriptCmd = m_pluginPath;
        scriptCmd.append(SCRIPTS_COMMANDS_PATH).append(cmdName).append(SCRIPT_EXT);
        string cmdParam = "";
        if (cmdName.compare(UptermCommands::start) == 0)
        {
            for (auto it=params.begin(); it!=params.end(); it++)
            {
                string paramName = (*it).first;
                string paramValue = (*it).second;
                if (paramValue.empty()) continue;
                if (paramName.compare(UptermCommands::password) == 0)
                {
                    cmdParam = cmdParam.append("--").append(paramName).append("="); // set to cmdParam directly since it's the first argument.
                    cmdParam.append(paramValue);
                    UTL_LOG_INFO("cmdParam: %s", cmdParam.c_str());
                    break;
                }
            }
        }
        string message;
        bool result;
        thread th1 = thread(CUptermPlugin::RunPluginScriptCmdOutput, scriptCmd, cmdParam, ref(result), ref(message));
        // if (cmdName.compare(UptermCommands::start) == 0)
        // {
            // UTL_LOG_INFO("3. start to run script");
            // th1.detach();
            // sleep(3);
        // }
        // else
        // {
            th1.join();
        // }

        if (!cmdAck) cmdAck = cJSON_CreateObject();
        cJSON_AddStringToObject(cmdAck, JKEY_NAME, cmdName.c_str());
        cJSON_AddStringToObject(cmdAck, JKEY_RESULT, message.c_str());
        cmdState = result? AckState::ACKED : AckState::ERRORED;
    }
    else
    {
        UTL_LOG_WARN("Unrecognized command.");
        if (!cmdAck) cmdAck = cJSON_CreateObject();
        cJSON_AddStringToObject(cmdAck, JKEY_NAME, cmdName.c_str());
        cJSON_AddStringToObject(cmdAck, JKEY_RESULT, "Unrecognized command.");
        cmdState = AckState::REJECTED;
    }
    return cmdState;
}

void CUptermPlugin::UpdateStates(bitset<4> updateMask)
{
    UTL_LOG_INFO("update mask: %s", updateMask.to_string<char,string::traits_type,string::allocator_type>().c_str());
    if ((updateMask & StateUpdated::status) == StateUpdated::status)
    {
        string cmdStatus = m_pluginPath;
        cmdStatus.append(SCRIPTS_STATES_PATH).append(UptermStates::status).append(SCRIPT_EXT);
        RunStatesScript(cmdStatus);
    }
    
    if ((updateMask & StateUpdated::web) == StateUpdated::web)
    {
        string cmdWeb = m_pluginPath;
        cmdWeb.append(SCRIPTS_STATES_PATH).append(UptermStates::web).append(SCRIPT_EXT);
        RunStatesScript(cmdWeb);
    }
    
    if ((updateMask & StateUpdated::ssh) == StateUpdated::ssh)
    {
        string cmdSsh = m_pluginPath;
        cmdSsh.append(SCRIPTS_STATES_PATH).append(UptermStates::ssh).append(SCRIPT_EXT);
        RunStatesScript(cmdSsh);
    }

    if ((updateMask & StateUpdated::version) == StateUpdated::version)
    {
        string cmdVersion = m_pluginPath;
        cmdVersion.append(SCRIPTS_STATES_PATH).append(UptermStates::version).append(SCRIPT_EXT);
        RunStatesScript(cmdVersion);
    }
}

cJSON *CUptermPlugin::AddStatusState()
{
    string scriptFile = m_pluginPath;
    return GetStatesOutput(m_pluginPath, UptermStates::status);
}

cJSON *CUptermPlugin::AddVersionState()
{
    string scriptFile = m_pluginPath;
    return GetStatesOutput(m_pluginPath, UptermStates::version);
}

cJSON *CUptermPlugin::AddWebLinkState()
{
    string scriptFile = m_pluginPath;
    return GetStatesOutput(m_pluginPath, UptermStates::web);
}

cJSON *CUptermPlugin::AddSshLinkState()
{
    string scriptFile = m_pluginPath;
    return GetStatesOutput(m_pluginPath, UptermStates::ssh);
}

bitset<4> CUptermPlugin::IsStateFilesChanged()
{
    bitset<4> result(0000UL);
    time_t newMTime;
    int rStatus = FileIsModified(m_statusOutput.c_str(), statusTime, newMTime);
    if (rStatus > 0)
    {
        statusTime = newMTime;
        result = result | StateUpdated::status;
#ifdef DEBUG
        UTL_LOG_INFO("status output is updated.");
#endif
    }
    int rVersion = FileIsModified(m_versionOutput.c_str(), versionTime, newMTime);
    if (rVersion > 0)
    {
        versionTime = newMTime;
        result = result | StateUpdated::version;
#ifdef DEBUG
        UTL_LOG_INFO("version output is updated.");
#endif
    }
    int rWeb = FileIsModified(m_webOutput.c_str(), webTime, newMTime);
    if (rWeb > 0)
    {
        webTime = newMTime;
        result = result | StateUpdated::web;
#ifdef DEBUG
        UTL_LOG_INFO("web output is updated.");
#endif
    }
    int rSsh = FileIsModified(m_sshOutput.c_str(), sshTime, newMTime);
    if (rSsh > 0)
    {
        sshTime = newMTime;
        result = result | StateUpdated::ssh;
#ifdef DEBUG
        UTL_LOG_INFO("ssh output is updated.");
#endif
    }

    UTL_LOG_INFO("result: %s", result.to_string<char,string::traits_type,string::allocator_type>().c_str());
    return result;
}

void CUptermPlugin::UpdateAlarmsData(const char *payload)
{
    if (payload == NULL) return;

    m_alertsStatus = AlertStatus::none;

    if (m_alarmUpdateObj)
    {
        UTL_LOG_INFO("going to delete m_alarmUpdateObj...");
        delete m_alarmUpdateObj;
        m_alarmUpdateObj = NULL;
    }
    m_alarmUpdateObj = new CAlarmUpdatePluginJson(payload, accessKey);
    UTL_LOG_INFO("new alarmUpdateObj object: %p", m_alarmUpdateObj);
    list<cJSON *> alarmsListUpterm = m_alarmUpdateObj->GetAlarms(moduleUpterm);
    list<cJSON *>::iterator ita;
    for (ita = alarmsListUpterm.begin(); ita != alarmsListUpterm.end(); ita++)
    {
        UTL_LOG_INFO("paramsJson: %s", cJSON_Print(*ita));
        cJSON *paramsJson = *ita;
        if (CPluginUtil::GetJSONBooleanFieldValue(paramsJson, JKEY_ENABLED))
        {
            string alarmName = CPluginUtil::GetJSONStringFieldValue(paramsJson, JKEY_NAME);
            cJSON *alarmParams = cJSON_Duplicate(cJSON_GetObjectItem(paramsJson, JKEY_PARAMS), cJSON_True);
            // if (alarmName.compare(UptermAlerts::cpu_overload) == 0)
            // {
            //     m_alertsStatus = m_alertsStatus | AlertStatus::alarm1;
            // }
        }
    }
    UTL_LOG_INFO("alarm mask: %s", AlertStatus::GetName(m_alertsStatus).c_str());
}

void CUptermPlugin::Init()
{
    UTL_LOG_INFO("Init");
    statusTime = 0;
    versionTime = 0;
    webTime = 0;
    sshTime = 0;
    m_pluginPath = string(PLUGINS_PATH).append(appGUID).append("/");
    m_statusOutput = m_pluginPath;
    m_statusOutput.append(SCRIPTS_STATES_PATH).append(UptermStates::status).append(SCRIPT_OUTPUT_EXT);
    m_versionOutput = m_pluginPath;
    m_versionOutput.append(SCRIPTS_STATES_PATH).append(UptermStates::version).append(SCRIPT_OUTPUT_EXT);
    m_webOutput = m_pluginPath;
    m_webOutput.append(SCRIPTS_STATES_PATH).append(UptermStates::web).append(SCRIPT_OUTPUT_EXT);
    m_sshOutput = m_pluginPath;
    m_sshOutput.append(SCRIPTS_STATES_PATH).append(UptermStates::ssh).append(SCRIPT_OUTPUT_EXT);
    m_alarmUpdateObj = NULL;
}

string CUptermPlugin::ReadOutput(const string outputName)
{
    if (outputName.empty()) return "Cannot find output";

    string output = "";
    string str;
    ifstream file(outputName);
    int i = 0;
    while (getline(file, str))
    {
        if (i++ > 0)
        {
            output.append("\n");
        }
        output.append(str);
    }

    return output;
}

bool CUptermPlugin::RunStatesScript(string scriptCmd)
{
    bool result = true;
    thread th1 = thread(CUptermPlugin::RunPluginScriptCmd, scriptCmd, "", ref(result));
    th1.join();
    return result;
}

cJSON *CUptermPlugin::GetStatesOutput(string pluginPath, string stateName)
{
    string scriptName = pluginPath;
    scriptName.append(SCRIPTS_STATES_PATH).append(stateName).append(SCRIPT_EXT);
    string scriptOutput = scriptName.substr(0, scriptName.find(SCRIPT_EXT)).append(SCRIPT_OUTPUT_EXT);
    string message = ReadOutput(scriptOutput);
    cJSON *testState = cJSON_CreateObject();
    cJSON_AddStringToObject(testState, JKEY_NAME, stateName.c_str());
    if (stateName.compare(UptermStates::web) == 0 || stateName.compare(UptermStates::ssh) == 0)
    {
        UTL_LOG_INFO(message.c_str());
        cJSON_AddItemToObject(testState, JKEY_VALUE, cJSON_Parse(message.c_str()));
    }
    else if (stateName.compare(UptermStates::version) == 0 || stateName.compare(UptermStates::status) == 0)
    {
        cJSON_AddStringToObject(testState, JKEY_VALUE, message.c_str());
    }

    return testState;
}

void CUptermPlugin::RunPluginScriptCmd(string scriptCmd, string cmdParam, bool &result)
{
    string scriptName = scriptCmd;
    if (!cmdParam.empty())
    {
        scriptCmd.append(" ").append(cmdParam);
    }
    int status = system(scriptCmd.c_str());
    UTL_LOG_INFO("run %s command, status = %x", scriptCmd.c_str(), status);
    if (status < 0)
    {
        result = false;
    }
    else
    {
        if (WIFEXITED(status))
        {
            int wexitstatus = WEXITSTATUS(status);
            if (0 == wexitstatus)
            {
                UTL_LOG_INFO("Program returned normally, exit code %d", wexitstatus);
                result = true;
            }
            else
            {
                stringstream ss;
                ss << wexitstatus;
                UTL_LOG_INFO("Run command fail, script exit code: %d\n", wexitstatus);
                result = false;
            }
        }
        else
        {
            result = false;
            UTL_LOG_INFO("Program exited abnormaly.");
        }
    }
}

void CUptermPlugin::RunPluginScriptCmdOutput(string scriptCmd, string cmdParam, bool &result, string &message)
{
    string scriptName = scriptCmd;
    if (!cmdParam.empty())
    {
        scriptCmd.append(" ").append(cmdParam);
    }
    int status = system(scriptCmd.c_str());
    UTL_LOG_INFO("run %s command, status = %d", scriptCmd.c_str(), status);
    string scriptOutput = scriptName.substr(0, scriptName.find(SCRIPT_EXT)).append(SCRIPT_OUTPUT_EXT);
    if (status < 0)
    {
        message.append("Error: ").append(strerror(errno)).append("occurred while running command ").append(scriptName);
        message.append("\n").append(ReadOutput(scriptOutput));
        result = false;
        UTL_LOG_INFO("%s.", message.c_str());
    }
    else
    {
        // UTL_LOG_INFO("exit code %d", WEXITSTATUS(status));
        if (WIFEXITED(status))
        {
            int wexitstatus = WEXITSTATUS(status);
            if (0 == wexitstatus)
            {
                message = ReadOutput(scriptOutput);
                UTL_LOG_INFO("Program returned normally, exit code %d", wexitstatus);
                result = true;
            }
            else
            {
                stringstream ss;
                ss << wexitstatus;
                message.append("Fail (").append(ss.str()).append("): ").append(ReadOutput(scriptOutput));
                UTL_LOG_INFO("Run command fail, script exit code: %d\n", wexitstatus);
                result = false;
            }
        }
        else
        {
            message.append("Program exited abnormaly.").append("\n").append(ReadOutput(scriptOutput));
            result = false;
            UTL_LOG_INFO("Program exited abnormaly.");
        }
    }
    // UTL_LOG_INFO("Result = %s", result ? "true" : "false");
}

string CUptermPlugin::RandomString(int len) {
    string s;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i)
    {
        s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return s;
}

int CUptermPlugin::FileIsModified(const char *path, time_t oldMTime, time_t &newMTime) {
    // UTL_LOG_INFO("file: %s, oldMTime: %d", path, oldMTime);
    struct stat fileStat;
    int err = stat(path, &fileStat);
    if (err != 0) {
        perror(" [file_is_modified] stat");
        exit(errno);
    }
    newMTime = fileStat.st_mtime;
    // UTL_LOG_INFO("file: %s, newMTime: %d", path, newMTime);
    return fileStat.st_mtime > oldMTime;
}
