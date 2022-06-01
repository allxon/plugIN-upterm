#include "Log.h"
#include <iostream>
#include "Util/Utl_Log.h"
#include "plugin_api/np_update_json.h"
#include "plugin_api/np_state_json.h"
#include "plugin_api/np_command_json.h"
#include "plugin_api/np_command_ack_json.h"
#include "json_validator.h"
#include "build_info.h"

#define ASIO_STANDALONE

std::string plugin_install_dir;
std::string np_update_str;

int getLock(void)
{
    struct flock fl;
    int fdlock;

    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;

    std::string pid_file("/var/run" + std::string(PLUGIN_NAME) + ".pid");
    if ((fdlock = open(pid_file.c_str(), O_WRONLY | O_CREAT, 0666)) == -1)
        return 0;

    if (fcntl(fdlock, F_SETLK, &fl) == -1)
        return 0;

    return 1;
}

std::string ReadOutput(const std::string &path)
{
    std::string output;
    std::string line_str;
    std::ifstream file(path);
    bool is_first_line = true;
    while (getline(file, line_str))
    {
        if (is_first_line)
            is_first_line = false;
        else
            output.append("\n");
        output.append(line_str);
    }

    return output;
}

bool RunCommand(const std::string &command)
{
    int status = system(command.c_str());
    UTL_LOG_INFO("run: %s, status = %x", command.c_str(), status);
    if (status < 0)
        return false;
    else
    {
        if (WIFEXITED(status))
        {
            int wexitstatus = WEXITSTATUS(status);
            if (0 == wexitstatus)
            {
                UTL_LOG_INFO("Program returned normally, exit code %d", wexitstatus);
                return true;
            }
            else
            {
                UTL_LOG_INFO("Run command fail, script exit code: %d\n", wexitstatus);
                return false;
            }
        }
        else
        {
            UTL_LOG_INFO("Program exited by signal.");
            return false;
        }
    }
}

bool RunPluginScript(const std::string &relative_path, const std::map<std::string, std::string> &arguments, std::string &output)
{
    auto command = plugin_install_dir + "/" + relative_path;
    for (const auto &[key, value] : arguments)
        command.append(" --" + key + " " + value);
    auto result = RunCommand(command);
    auto output_path = relative_path.substr(0, relative_path.find(".sh")).append(".output");
    output = ReadOutput(plugin_install_dir + "/" + output_path);
    return result;
}

std::string getJsonFromFile(const std::string &path)
{
    std::string output;
    std::ifstream myfile(path);
    if (myfile.is_open())
    {
        std::string line;
        while (std::getline(myfile, line))
            output.append(line + "\n");
        myfile.close();
    }
    else
        std::cout << "Unable to open file";
    return output;
}

#include "websocketpp/config/asio_client.hpp"
#include "websocketpp/client.hpp"

using namespace Allxon;
typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

class WebSocketClient
{
private:
    context_ptr OnTLSInit(websocketpp::connection_hdl hdl)
    {
        UTL_LOG_INFO("OnTLSInit");
        context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12);

        ctx->set_options(asio::ssl::context::default_workarounds |
                         asio::ssl::context::no_sslv2 |
                         asio::ssl::context::no_sslv3 |
                         asio::ssl::context::single_dh_use);

        ctx->set_verify_mode(asio::ssl::verify_none);
        return ctx;
    }

    void OnOpen(websocketpp::connection_hdl hdl)
    {
        UTL_LOG_INFO("OnOpen");
        set_connection_established(true);
        SendNotifyPluginUpdate();
    }
    void OnClose(websocketpp::connection_hdl hdl)
    {
        UTL_LOG_INFO("OnClose");
        m_endpoint.close(hdl, websocketpp::close::status::normal, "Connection closed");
        m_endpoint.stop();
        set_connection_established(false);
    }
    void OnFail(websocketpp::connection_hdl hdl)
    {
        UTL_LOG_WARN("OnFail");
        m_endpoint.get_alog().write(websocketpp::log::alevel::app, "Connection Failed");
        m_endpoint.close(hdl, websocketpp::close::status::normal, "Connection Failed.");
        m_endpoint.stop();
        set_connection_established(false);
    }
    void OnMessage(websocketpp::connection_hdl hdl, client::message_ptr msg)
    {
        const auto payload = msg->get_payload();
        UTL_LOG_INFO("OnMessage");
        UTL_LOG_INFO(payload.c_str());
        std::string get_method;
        if (!m_json_validator->Verify(payload, get_method))
        {
            UTL_LOG_ERROR("OnMessage payload verify failed");
            return;
        }
        PluginAPI plugin_api;
        plugin_api.ImportFromString(payload);
        if (get_method == "v2/notifyPluginCommand")
        {
            NPCommandJson np_cmd_json;
            np_cmd_json.ImportFromString(payload);
            UTL_LOG_INFO("get command id: %s", np_cmd_json.command_id().c_str());

            auto receive_commands = np_cmd_json.commands_json();
            std::vector<CommandAckCmdAckJson> cmds_accept;
            for (const auto &cmd : receive_commands)
            {
                cmds_accept.push_back({cmd.name()});
            }
            NPCommandAckJson np_cmd_accept_json(PLUGIN_APP_GUID, "", np_cmd_json.command_id(),
                                                np_cmd_json.command_source(), np_cmd_json.module_name(),
                                                Allxon::NPCommandAckJson::CommandState::ACCEPTED,
                                                cmds_accept);
            PushCommandQueue(m_cmd_accept_queue, np_cmd_accept_json);

            const auto &np_update_commands = m_json_validator->np_update_json().modules_json().front().commands_json();
            std::vector<CommandAckCmdAckJson> cmds_ack;
            for (const auto &receive_cmd : receive_commands)
            {
                auto match_cmd = std::find_if(std::begin(np_update_commands), std::end(np_update_commands), [&](const UpdateCommandJson &update_cmd)
                                              { return update_cmd.name() == receive_cmd.name(); });
                if (match_cmd == std::end(np_update_commands))
                {
                    UTL_LOG_WARN("can't find matched cmd");
                    return;
                }

                std::map<std::string, std::string> arguments;
                for (const auto &param : receive_cmd.params_json())
                    arguments[param.name()] = param.value_string();
                std::string cmd_output;
                bool cmd_result = RunPluginScript("scripts/commands/" + receive_cmd.name() + ".sh", arguments, cmd_output);
                cmds_ack.push_back({receive_cmd.name(), cmd_output});
            }

            NPCommandAckJson np_cmd_ack_json(PLUGIN_APP_GUID, "", np_cmd_json.command_id(),
                                             np_cmd_json.command_source(), np_cmd_json.module_name(),
                                             Allxon::NPCommandAckJson::CommandState::ACKED,
                                             cmds_ack);
            PushCommandQueue(m_cmd_ack_queue, np_cmd_ack_json);
        }
        else
        {
            UTL_LOG_ERROR("OnMessage payload unknown method");
        }
    }
    void SendNotifyPluginUpdate()
    {
        UTL_LOG_INFO("SendNotifyPluginUpdate");
        auto output_string = m_json_validator->np_update_json().ExportToString();
        if (!m_json_validator->Sign(output_string))
        {
            UTL_LOG_ERROR(m_json_validator->error_message().c_str());
            return;
        }
        m_endpoint.send(m_hdl, output_string.c_str(), websocketpp::frame::opcode::TEXT);
    }

    void SendPluginStatesMetrics()
    {
        UTL_LOG_INFO("SendPluginStateMetrics");
        const auto &states = m_json_validator->np_update_json().modules_json().front().states_json();

        std::vector<ValueParamJson> value_params_json;
        for (const auto &state : states)
        {
            std::string value_output;
            if (!RunPluginScript("scripts/states/" + state.name() + ".sh", {}, value_output))
                value_output = "N/A";
            value_params_json.push_back({state.name(), value_output});
        }

        NPStateJson state_json(PLUGIN_APP_GUID, "", PLUGIN_NAME, value_params_json);
        auto output_str = state_json.ExportToString();
        if (!m_json_validator->Sign(output_str))
        {
            UTL_LOG_ERROR(m_json_validator->error_message().c_str());
            return;
        }

        m_endpoint.send(m_hdl, output_str.c_str(), websocketpp::frame::opcode::TEXT);
    }

    void SendPluginCommandAck(std::queue<Allxon::NPCommandAckJson> &queue)
    {
        if (queue.empty())
            return;
        UTL_LOG_INFO("SendPluginCommandAck");
        NPCommandAckJson np_cmd_ack_json;
        while (PopCommandQueue(queue, np_cmd_ack_json))
        {
            auto output_str = np_cmd_ack_json.ExportToString();
            if (!m_json_validator->Sign(output_str))
            {
                UTL_LOG_ERROR(m_json_validator->error_message().c_str());
                return;
            }

            m_endpoint.send(m_hdl, output_str.c_str(), websocketpp::frame::opcode::TEXT);
        }
    }

    bool connection_established()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_connection_established;
    }
    void set_connection_established(bool value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connection_established = value;
    }

    void PushCommandQueue(std::queue<Allxon::NPCommandAckJson> &queue, const Allxon::NPCommandAckJson &command)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        queue.push(command);
    }

    bool PopCommandQueue(std::queue<Allxon::NPCommandAckJson> &queue, Allxon::NPCommandAckJson &pop_command)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (queue.empty())
            return false;
        pop_command = queue.front();
        queue.pop();
        return true;
    }

    client m_endpoint;
    websocketpp::connection_hdl m_hdl;
    std::mutex m_mutex;
    websocketpp::lib::shared_ptr<std::thread> m_run_thread;
    websocketpp::lib::shared_ptr<std::thread> m_send_thread;
    bool m_connection_established = false;
    std::shared_ptr<Allxon::JsonValidator> m_json_validator;
    std::queue<Allxon::NPCommandAckJson> m_cmd_accept_queue;
    std::queue<Allxon::NPCommandAckJson> m_cmd_ack_queue;

public:
    WebSocketClient(std::shared_ptr<Allxon::JsonValidator> json_validator) : m_json_validator(json_validator)
    {
        m_endpoint.set_reuse_addr(true);
        m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
        m_endpoint.clear_access_channels(websocketpp::log::elevel::all);
        m_endpoint.clear_error_channels(websocketpp::log::alevel::all);
        m_endpoint.clear_error_channels(websocketpp::log::elevel::all);
        m_endpoint.set_tls_init_handler(bind(&WebSocketClient::OnTLSInit, this, std::placeholders::_1));
        m_endpoint.set_open_handler(bind(&WebSocketClient::OnOpen, this, std::placeholders::_1));
        m_endpoint.set_fail_handler(bind(&WebSocketClient::OnFail, this, std::placeholders::_1));
        m_endpoint.set_message_handler(bind(&WebSocketClient::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
        m_endpoint.set_close_handler(bind(&WebSocketClient::OnClose, this, std::placeholders::_1));
        m_endpoint.init_asio();
        m_endpoint.start_perpetual();
        m_run_thread.reset(new std::thread(&client::run, &m_endpoint));
    }
    ~WebSocketClient()
    {
    }

    void Connect(std::string const &uri)
    {
        // the event loop starts
        websocketpp::lib::error_code ec;
        client::connection_ptr con = m_endpoint.get_connection(uri, ec);
        if (ec)
        {
            UTL_LOG_ERROR("Connect initialization error: %s", ec.message().c_str());
            std::cout << "Connect initialization error: " << ec.message() << std::endl;
            return;
        }
        m_hdl = con->get_handle();
        m_endpoint.connect(con);
    }

    void RunSendingLoop()
    {
        int count = 0;
        while (true)
        {
            sleep(1);
            if (!connection_established())
                continue;

            SendPluginCommandAck(m_cmd_accept_queue);
            SendPluginCommandAck(m_cmd_ack_queue);
            if (++count == 60)
            {
                SendPluginStatesMetrics();
                count = 0;
            }
        }
    }
};

int main(int argc, char **argv)
{
    Log start; // start Logging
    UTL_LOG_INFO("PLUGIN_VERSION: %s", PLUGIN_VERSION);

#ifndef DEBUG
    if (!getLock()) // Check single instance app
    {
        UTL_LOG_WARN("Process already running!\n", stderr);
        return 1;
    }
#endif

    UTL_LOG_INFO("argc = %d, argv[1] = %s", argc, argv[1]);
    if (argc == 1)
    {
        UTL_LOG_WARN("Please provide a plugin config file, e.g. \'device_plugin plugin_config.json\'.");
        return 1;
    }
    else if (argc > 2)
    {
        UTL_LOG_WARN("Wrong arguments. Usage: device_plugin [config file]");
        return 1;
    }
    plugin_install_dir = std::string(argv[1]);
    np_update_str = getJsonFromFile(plugin_install_dir + "/plugin_update_template.json");
    auto json_validator = std::make_shared<JsonValidator>(PLUGIN_NAME, PLUGIN_APP_GUID,
                                                          PLUGIN_ACCESS_KEY, PLUGIN_VERSION,
                                                          np_update_str);
    WebSocketClient web_client(json_validator);
    web_client.Connect("wss://127.0.0.1:55688");
    web_client.RunSendingLoop();
    return 0;
}
