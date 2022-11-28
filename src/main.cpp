#include <iostream>
#include <fstream>
#include <fcntl.h>
#include "cJSON.h"
#include "json_validator.h"
#include "build_info.h"

#define ASIO_STANDALONE

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

class Util
{
public:
    static std::string getJsonFromFile(const std::string &path)
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

    static std::string plugin_install_dir;
};

bool RunCommand(const std::string &command)
{
    std::cout << "run: " << command.c_str() << std::endl;
    int status = system(command.c_str());
    std::cout << "return status: " << status << std::endl;
    if (status < 0)
        return false;
    else
    {
        if (WIFEXITED(status))
        {
            int wexitstatus = WEXITSTATUS(status);
            if (0 == wexitstatus)
            {
                std::cout << "Program returned normally, exit code: " << wexitstatus << std::endl;
                return true;
            }
            else
            {
                std::cout << "Run command fail, script exit code: " << wexitstatus << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << "Program exited by signal." << std::endl;
            return false;
        }
    }
}

bool RunPluginScript(const std::string &relative_path, const std::map<std::string, std::string> &arguments, std::string &output)
{
    auto command = Util::plugin_install_dir + "/" + relative_path;
    for (const auto &[key, value] : arguments)
        command.append(" --" + key + " " + value);
    auto result = RunCommand(command);
    auto output_path = relative_path.substr(0, relative_path.find(".sh")).append(".output");
    output = ReadOutput(Util::plugin_install_dir + "/" + output_path);
    return result;
}

class WebSocketClient
{
private:
    context_ptr OnTLSInit(websocketpp::connection_hdl hdl)
    {
        std::cout << "OnTLSInit" << std::endl;
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
        std::cout << "OnOpen" << std::endl;
        set_connection_established(true);
        SendNotifyPluginUpdate();
    }
    void OnClose(websocketpp::connection_hdl hdl)
    {
        std::cout << "OnClose" << std::endl;
        m_endpoint.close(hdl, websocketpp::close::status::normal, "Connection closed");
        m_endpoint.stop();
        set_connection_established(false);
        exit(0);
    }
    void OnFail(websocketpp::connection_hdl hdl)
    {
        std::cout << "OnFail" << std::endl;
        m_endpoint.get_alog().write(websocketpp::log::alevel::app, "Connection Failed");
        m_endpoint.close(hdl, websocketpp::close::status::normal, "Connection Failed.");
        m_endpoint.stop();
        set_connection_established(false);
    }
    void OnMessage(websocketpp::connection_hdl hdl, client::message_ptr msg)
    {
        const auto payload = msg->get_payload();
        std::cout << "OnMessage" << std::endl;
        std::cout << payload.c_str() << std::endl;

        // Handle JSON-RPC error object
        auto payload_cjson = cJSON_Parse(payload.c_str());
        if (cJSON_HasObjectItem(payload_cjson, "error"))
        {
            auto error_cjson = cJSON_GetObjectItemCaseSensitive(payload_cjson, "error");
            auto error_code_cjson = cJSON_GetObjectItemCaseSensitive(error_cjson, "code");
            auto error_code = cJSON_GetStringValue(error_code_cjson);
            auto error_msg_cjson = cJSON_GetObjectItemCaseSensitive(error_cjson, "message");
            auto error_msg = cJSON_GetStringValue(error_msg_cjson);
            printf("Received JSON-RPC error object, error code: %s, error_message: %s\n", error_code, error_msg);
            cJSON_Delete(payload_cjson);
            return;
        }
        cJSON_Delete(payload_cjson);

        std::string get_method;
        if (!m_json_validator->Verify(payload, get_method))
        {
            // if get offline error from agent core, need reconnect
            std::cout << m_json_validator->error_message() << std::endl;
            exit(0);
        }
        if (get_method == "v2/notifyPluginCommand")
        {
            std::cout << "Get Method:" << get_method << std::endl;
            auto cmd_cjson = cJSON_Parse(payload.c_str());
            auto params_cjson = cJSON_GetObjectItemCaseSensitive(cmd_cjson, "params");
            auto command_id_cjson = cJSON_GetObjectItemCaseSensitive(params_cjson, "commandId");
            std::cout << "get command id: " << command_id_cjson->valuestring << std::endl;
            PushCommandQueue(m_cmd_queue, payload);
            cJSON_Delete(cmd_cjson);
        }
        else
        {
            std::cerr << "OnMessage payload unknown method" << std::endl;
        }
    }
    void SendNotifyPluginUpdate()
    {
        std::cout << "SendNotifyPluginUpdate" << std::endl;
        std::string notify_plugin_update = Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_update_template.json");
        auto np_update_cjson = cJSON_Parse(notify_plugin_update.c_str());
        auto output_char = cJSON_Print(np_update_cjson);
        std::string output_string(output_char);
        delete output_char;
        cJSON_Delete(np_update_cjson);
        if (!m_json_validator->Sign(output_string))
        {
            std::cout << m_json_validator->error_message().c_str() << std::endl;
            return;
        }
        m_endpoint.send(m_hdl, output_string.c_str(), websocketpp::frame::opcode::TEXT);
        std::cout << "Send:" << output_string << std::endl;
    }

    void SendPluginStatesMetrics()
    {
        std::cout << "SendPluginStateMetrics" << std::endl;
        std::string notify_plugin_update = Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_update_template.json");
        auto np_update_cjson = cJSON_Parse(notify_plugin_update.c_str());
        auto params_cjson = cJSON_GetObjectItemCaseSensitive(np_update_cjson, "params");
        auto modules_cjson = cJSON_GetObjectItemCaseSensitive(params_cjson, "modules");
        auto module_cjson = cJSON_GetArrayItem(modules_cjson, 0);
        auto states_cjson = cJSON_GetObjectItemCaseSensitive(module_cjson, "states");
        std::string notify_plugin_state = Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_state.json");
        auto np_state_cjson = cJSON_Parse(notify_plugin_state.c_str());
        auto temp_params_cjson = cJSON_GetObjectItemCaseSensitive(np_state_cjson, "params");
        auto temp_states_cjson = cJSON_GetObjectItemCaseSensitive(temp_params_cjson, "states");
        const cJSON *state_cjson = NULL;
        cJSON_ArrayForEach(state_cjson, states_cjson)
        {
            auto state_name_cjson = cJSON_GetObjectItemCaseSensitive(state_cjson, "name"); 
            auto state_name_str = std::string(cJSON_GetStringValue(state_name_cjson));
            std::string value_output;
            if (!RunPluginScript("scripts/states/" + state_name_str + ".sh", {}, value_output))
                value_output = "N/A";
            auto state_output_cjson = cJSON_CreateObject();
            cJSON_AddStringToObject(state_output_cjson, "name", state_name_str.c_str());
            cJSON_AddStringToObject(state_output_cjson, "value", value_output.c_str());
            cJSON_AddItemToArray(temp_states_cjson, state_output_cjson);
        }
        auto output_char = cJSON_Print(np_state_cjson);
        std::string output_string(output_char);
        delete output_char;
        cJSON_Delete(np_state_cjson); 
        cJSON_Delete(np_update_cjson); 
        if (!m_json_validator->Sign(output_string))
        {
            std::cerr << m_json_validator->error_message().c_str() << std::endl;
            return;
        }

        m_endpoint.send(m_hdl, output_string.c_str(), websocketpp::frame::opcode::TEXT);
        std::cout << "Send:" << output_string << std::endl;
    }

    void SendPluginCommandAck(std::queue<std::string> &queue)
    {
        if (queue.empty())
            return;
        std::cout << "SendPluginCommandAck" << std::endl;
        std::string np_cmd_ack_str;
        while (PopCommandQueue(queue, np_cmd_ack_str))
        {
            std::string get_method;
            m_json_validator->Verify(np_cmd_ack_str, get_method);
            auto cmd_cjson = cJSON_Parse(np_cmd_ack_str.c_str());
            auto params_cjson = cJSON_GetObjectItemCaseSensitive(cmd_cjson, "params");
            auto command_id_cjson = cJSON_GetObjectItemCaseSensitive(params_cjson, "commandId");
            auto commands_cjson = cJSON_GetObjectItemCaseSensitive(params_cjson, "commands");
            auto command_cjson = cJSON_GetArrayItem(commands_cjson, 0);
            auto command_name_cjson= cJSON_GetObjectItemCaseSensitive(command_cjson, "name");
            auto command_params_cjson= cJSON_GetObjectItemCaseSensitive(command_cjson, "params");
            auto command_name_str = std::string(command_name_cjson->valuestring);

            auto cmd_accept = Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_command_ack.json");
            auto cmd_accept_cjson = cJSON_Parse(cmd_accept.c_str());
            auto accept_params_cjson = cJSON_GetObjectItemCaseSensitive(cmd_accept_cjson, "params");
            auto accept_command_id_cjson = cJSON_GetObjectItemCaseSensitive(accept_params_cjson, "commandId");
            auto accept_command_state_cjson = cJSON_GetObjectItemCaseSensitive(accept_params_cjson, "commandState");
            auto accept_command_acks_cjson = cJSON_GetObjectItemCaseSensitive(accept_params_cjson, "commandAcks");
            auto accept_states_cjson = cJSON_GetObjectItemCaseSensitive(accept_params_cjson, "states");
            auto accept_command_ack_cjson = cJSON_GetArrayItem(accept_command_acks_cjson, 0);
            auto accept_cmd_name_cjson= cJSON_GetObjectItemCaseSensitive(accept_command_ack_cjson, "name");
            auto accept_result_cjson = cJSON_GetObjectItemCaseSensitive(accept_command_ack_cjson, "result");

            cJSON_SetValuestring(accept_command_id_cjson, command_id_cjson->valuestring);
            cJSON_SetValuestring(accept_cmd_name_cjson, command_name_str.c_str());
            cJSON_SetValuestring(accept_command_state_cjson, "ACCEPTED");
            cJSON_AddStringToObject(accept_result_cjson, "output", "received command");
            char* cmd_accept_str_ptr = cJSON_Print(cmd_accept_cjson);
            auto cmd_accept_str = std::string(cmd_accept_str_ptr);
            delete cmd_accept_str_ptr;

            if (!m_json_validator->Sign(cmd_accept_str))
            {
                std::cerr << m_json_validator->error_message().c_str() << std::endl;
                cJSON_Delete(cmd_cjson);
                cJSON_Delete(cmd_accept_cjson);
                return;
            }

            m_endpoint.send(m_hdl, cmd_accept_str.c_str(), websocketpp::frame::opcode::TEXT);
            std::cout << "Send:" << std::endl;
            std::cout << cmd_accept_str << std::endl;

            std::map<std::string, std::string> arguments;
            const cJSON *cmd_param_cjson = NULL;
            cJSON_ArrayForEach(cmd_param_cjson, command_params_cjson) {
                auto name_cjson = cJSON_GetObjectItemCaseSensitive(cmd_param_cjson, "name");
                auto value_cjson = cJSON_GetObjectItemCaseSensitive(cmd_param_cjson, "value");
                arguments[std::string(name_cjson->valuestring)] = std::string(value_cjson->valuestring);
            }
            std::string cmd_output;
            bool cmd_result = RunPluginScript("scripts/commands/" + command_name_str + ".sh", arguments, cmd_output);
            cJSON_SetValuestring(accept_command_state_cjson, "ACKED");
            cJSON_AddStringToObject(accept_result_cjson, "response", cmd_output.c_str());

            std::string notify_plugin_update = Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_update_template.json");
            auto np_update_cjson = cJSON_Parse(notify_plugin_update.c_str());
            auto np_params_cjson = cJSON_GetObjectItemCaseSensitive(np_update_cjson, "params");
            auto modules_cjson = cJSON_GetObjectItemCaseSensitive(np_params_cjson, "modules");
            auto module_cjson = cJSON_GetArrayItem(modules_cjson, 0);
            auto states_cjson = cJSON_GetObjectItemCaseSensitive(module_cjson, "states");

            const cJSON *state_cjson = NULL;
            cJSON_ArrayForEach(state_cjson, states_cjson)
            {
                auto state_name_cjson = cJSON_GetObjectItemCaseSensitive(state_cjson, "name"); 
                auto state_name_str = std::string(cJSON_GetStringValue(state_name_cjson));
                std::string value_output;
                if (!RunPluginScript("scripts/states/" + state_name_str + ".sh", {}, value_output))
                    value_output = "N/A";
                auto state_output_cjson = cJSON_CreateObject();
                cJSON_AddStringToObject(state_output_cjson, "name", state_name_str.c_str());
                cJSON_AddStringToObject(state_output_cjson, "value", value_output.c_str());
                cJSON_AddItemToArray(accept_states_cjson, state_output_cjson);
            }

            char *cmd_ack_str_ptr = cJSON_Print(cmd_accept_cjson);
            auto cmd_ack_str = std::string(cmd_ack_str_ptr);
            delete cmd_ack_str_ptr;

            if (!m_json_validator->Sign(cmd_ack_str))
            {
                std::cerr << m_json_validator->error_message().c_str() << std::endl;
                cJSON_Delete(cmd_cjson);
                cJSON_Delete(cmd_accept_cjson);
                cJSON_Delete(np_update_cjson);
                return;
            }

            m_endpoint.send(m_hdl, cmd_ack_str.c_str(), websocketpp::frame::opcode::TEXT);
            std::cout << "Send:" << std::endl;
            std::cout << cmd_ack_str << std::endl;
            cJSON_Delete(cmd_cjson);
            cJSON_Delete(cmd_accept_cjson);
            cJSON_Delete(np_update_cjson);
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

    void PushCommandQueue(std::queue<std::string> &queue, std::string data)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        queue.push(data);
    }

    bool PopCommandQueue(std::queue<std::string> &queue, std::string &pop_data)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (queue.empty())
            return false;
        pop_data = queue.front();
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
    std::queue<std::string> m_cmd_queue;
    std::string m_url;

public:
    WebSocketClient(std::shared_ptr<Allxon::JsonValidator> json_validator, const std::string &url) 
        : m_json_validator(json_validator), m_url(url)
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
            std::cerr << "Connect initialization error: " << ec.message() << std::endl;
            return;
        }
        m_hdl = con->get_handle();
        m_endpoint.connect(con);
    }

    void RunSendingLoop()
    {
        Connect(m_url);
        int count = 0;
        while (true)
        {
            sleep(1);
            if (!connection_established())
                continue;

            SendPluginCommandAck(m_cmd_queue);
            if (++count == 60)
            {
                SendPluginStatesMetrics();
                count = 0;
            }
        }
    }
};

std::string Util::plugin_install_dir = "";

int main(int argc, char **argv)
{
    std::cout << "PLUGIN_VERSION: " << PLUGIN_VERSION << std::endl;
    if (argc == 1)
    {
        std::cerr << "Please provide a plugin config file, e.g. \'device_plugin plugin_config.json\'." << std::endl;
        return 1;
    }
    else if (argc > 2)
    {
        std::cerr << "Wrong arguments. Usage: device_plugin [config file]" << std::endl;
        return 1;
    }
    Util::plugin_install_dir = std::string(argv[1]);
    auto np_update_json = Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_update_template.json");
    auto json_validator = std::make_shared<JsonValidator>(PLUGIN_NAME, PLUGIN_APP_GUID,
                                                          PLUGIN_ACCESS_KEY, PLUGIN_VERSION,
                                                          np_update_json);
    WebSocketClient web_client(json_validator, "wss://127.0.0.1:55688");
    web_client.RunSendingLoop();
    return 0;
}
