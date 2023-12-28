#include <iostream>
#include <fstream>
#include <fcntl.h>
#include "octo/octo.h"

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
    if (output.size() > 1500)
    {
        output = output.substr(0, 1500);
        output.append("...");
    }
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
        set_connection_established(false);
        exit(0);
    }
    void OnFail(websocketpp::connection_hdl hdl)
    {
        std::cout << "OnFail" << std::endl;
        m_endpoint.get_alog().write(websocketpp::log::alevel::app, "Connection Failed");
        set_connection_established(false);
        exit(1);
    }
    void OnMessage(websocketpp::connection_hdl hdl, client::message_ptr msg)
    {
        const auto payload = AJson::create(msg->get_payload());
        std::cout << "on_message" << std::endl;
        std::cout << payload.print(false) << std::endl;

        // Handle JSON-RPC error object
        if (payload.has_object_item("error"))
        {
            auto error = payload.item("error");
            printf("Received JSON-RPC error object, error code: %s, error_message: %s\n",
                   error.item("code").string().c_str(), error.item("message").string().c_str());
            return;
        }

        std::string get_method;
        auto content = payload.print(false);
        if (!octo_->verify(content, get_method))
        {
            std::cout << octo_->error_message() << std::endl;
            std::cerr << "Error code: " << std::to_string(static_cast<int>(octo_->error_code())) << std::endl;
            return;
        }

        if (get_method == "v2/notifyPluginCommand")
        {
            std::cout << "Get Method:" << get_method << std::endl;
            auto cmd_id = payload.item("params").item("commandId").string();
            std::cout << "get command id: " << cmd_id << std::endl;
            PushCommandQueue(m_cmd_queue, content);
        }
        else
        {
            std::cerr << "OnMessage payload unknown method" << std::endl;
        }
    }
    void SendNotifyPluginUpdate()
    {
        std::cout << "SendNotifyPluginUpdate" << std::endl;
        auto np_update = AJson::create(Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_update_template.json")).print(false);
        if (!octo_->sign(np_update))
        {
            std::cout << octo_->error_message().c_str() << std::endl;
            std::cerr << "Error code: " << std::to_string(static_cast<int>(octo_->error_code())) << std::endl;
            return;
        }
        m_endpoint.send(m_hdl, np_update.c_str(), websocketpp::frame::opcode::TEXT);
        std::cout << "Send:" << np_update << std::endl;
    }

    void SendPluginStatesMetrics()
    {
        std::cout << "SendPluginStateMetrics" << std::endl;
        auto np_update_state = AJson::create(Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_update_template.json")).item("params").item("modules").item(0).item("states");
        auto np_states_temp = AJson::create(Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_state.json"));

        for (int i = 0; i < np_update_state.array_size(); ++i)
        {
            auto state_name = np_update_state.item(i).item("name").string();
            std::string value_output;
            if (!RunPluginScript("scripts/states/" + state_name + ".sh", {}, value_output))
                value_output = "N/A";
            auto state_output_json = AJson::create_object({{"name", state_name.c_str()},
                                                           {"value", value_output.c_str()}});
            np_states_temp["params"]["states"].add_item_to_array(state_output_json);
        }
        auto np_state = np_states_temp.print(false);
        if (!octo_->sign(np_state))
        {
            std::cerr << octo_->error_message().c_str() << std::endl;
            std::cerr << "Error code: " << std::to_string(static_cast<int>(octo_->error_code())) << std::endl;
            return;
        }

        m_endpoint.send(m_hdl, np_state.c_str(), websocketpp::frame::opcode::TEXT);
        std::cout << "Send:" << np_state << std::endl;
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
            octo_->verify(np_cmd_ack_str, get_method);
            auto np_cmd_ack_json = AJson::create(np_cmd_ack_str);
            auto command_json = np_cmd_ack_json.item("params").item("commands").item(0);

            auto cmd_accept = Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_command_ack.json");
            auto cmd_accept_json = AJson::create(cmd_accept);
            cmd_accept_json["params"]["commandId"].set_string(np_cmd_ack_json.item("params").item("commandId").string());
            cmd_accept_json["params"]["commandState"].set_string("ACCEPTED");
            cmd_accept_json["params"]["commandAcks"][0]["name"].set_string(command_json.item("name").string());
            cmd_accept_json["params"]["commandAcks"][0]["result"].add_item_to_object("output", "received command");
            auto np_cmd_accept = cmd_accept_json.print(false);
            if (!octo_->sign(np_cmd_accept))
            {
                std::cerr << octo_->error_message().c_str() << std::endl;
                std::cerr << "Error code: " << std::to_string(static_cast<int>(octo_->error_code())) << std::endl;
                return;
            }

            m_endpoint.send(m_hdl, np_cmd_accept.c_str(), websocketpp::frame::opcode::TEXT);
            std::cout << "Send:" << std::endl;
            std::cout << np_cmd_accept << std::endl;

            std::map<std::string, std::string> arguments;
            if (command_json.has_object_item("params")) {
                auto command_params_json = command_json.item("params");
                for(int i = 0; i < command_params_json.array_size(); ++i)
                {
                    auto name = command_params_json.item(i).item("name").string();
                    auto value = command_params_json.item(i).item("value").string();
                    arguments[name] = value;
                }
            }
            std::string cmd_output;
            bool cmd_result = RunPluginScript("scripts/commands/" + command_json.item("name").string() + ".sh", arguments, cmd_output);
            cmd_accept_json["params"]["commandState"].set_string(cmd_result ? "ACKED" : "ERRORED");
            cmd_accept_json["params"]["commandAcks"][0]["result"].add_item_to_object("response", cmd_output.c_str());

            auto np_update_json = AJson::create(Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_update_template.json"));
            auto states_json = np_update_json.item("params").item("modules").item(0).item("states");

            for (int i = 0; i < states_json.array_size(); ++i)
            {
                auto state_name = states_json.item(i).item("name").string();
                std::string value_output;
                if (!RunPluginScript("scripts/states/" + state_name + ".sh", {}, value_output))
                    value_output = "N/A";
                auto state_output_json = AJson::create_object({{"name", state_name.c_str()},
                                                               {"value", value_output.c_str()}});
                cmd_accept_json["params"]["states"].add_item_to_array(state_output_json);
            }

            auto cmd_ack_str = cmd_accept_json.print(false);
            if (!octo_->sign(cmd_ack_str))
            {
                std::cerr << octo_->error_message().c_str() << std::endl;
                std::cerr << "Error code: " << std::to_string(static_cast<int>(octo_->error_code())) << std::endl;
                return;
            }

            m_endpoint.send(m_hdl, cmd_ack_str.c_str(), websocketpp::frame::opcode::TEXT);
            std::cout << "Send:" << std::endl;
            std::cout << cmd_ack_str << std::endl;
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
    std::shared_ptr<Allxon::Octo> octo_;
    std::queue<std::string> m_cmd_queue;
    std::string m_url;

public:
    WebSocketClient(std::shared_ptr<Allxon::Octo> octo, const std::string &url)
        : octo_(octo), m_url(url)
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
    WebSocketClient web_client(std::make_shared<Octo>(
                                   PLUGIN_NAME, PLUGIN_APP_GUID,
                                   PLUGIN_ACCESS_KEY, PLUGIN_VERSION,
                                   Util::getJsonFromFile(Util::plugin_install_dir + "/plugin_update_template.json")),
                               "wss://127.0.0.1:55688");
    web_client.RunSendingLoop();
    return 0;
}
