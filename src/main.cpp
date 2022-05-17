#include "Log.h"
#include <dirent.h>
#include <typeinfo>
#include <csignal>
#include "Util/Utl_Log.h"
#include "plugin_api/np_update_json.h"
#include "json_validator.h"
#include "build_info.h"

// int getLock(void)
// {
//     struct flock fl;
//     int fdlock;

//     fl.l_type = F_WRLCK;
//     fl.l_whence = SEEK_SET;
//     fl.l_start = 0;
//     fl.l_len = 1;

//     std::string pid_file("/var/run" + std::string(CMAKE_PROJECT_NAME) + ".pid");
//     if ((fdlock = open(pid_file.c_str(), O_WRONLY | O_CREAT, 0666)) == -1)
//         return 0;

//     if (fcntl(fdlock, F_SETLK, &fl) == -1)
//         return 0;

//     return 1;
// }

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

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

class WebSocketClient
{
private:
    context_ptr OnTLSInit(websocketpp::connection_hdl hdl)
    {
        UTL_LOG_INFO("OnTLSInit");
        context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);

        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::no_sslv3 |
                         boost::asio::ssl::context::single_dh_use);

        ctx->set_verify_mode(boost::asio::ssl::verify_none);
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
        UTL_LOG_INFO("OnMessage");
    }
    void SendNotifyPluginUpdate()
    {
        UTL_LOG_INFO("SendNotifyPluginUpdate");
        auto output_string = m_json_validator->np_update_json().ExportToString();
        m_json_validator->Sign(output_string);
        m_endpoint.send(m_hdl, output_string.c_str(), websocketpp::frame::opcode::TEXT);
    }

    void SendPluginStatesMetrics()
    {
        UTL_LOG_INFO("SendPluginStateMetrics");
        // std::string methodString = "v2/notifyPluginMetric";
        // std::string outstring;
        // Conver2SignedData(PLUGIN_AGENT_GUID, oss.str(), methodString, &outstring);
        // m_endpoint.send(m_hdl, outstring.c_str(), websocketpp::frame::opcode::TEXT);

        // methodString = "v2/notifyPluginState";

        // Conver2SignedData(PLUGIN_AGENT_GUID, oss.str(), methodString, &outstring);
        // m_endpoint.send(m_hdl, outstring.c_str(), websocketpp::frame::opcode::TEXT);
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

    client m_endpoint;
    websocketpp::connection_hdl m_hdl;
    std::mutex m_mutex;
    websocketpp::lib::shared_ptr<std::thread> m_run_thread;
    websocketpp::lib::shared_ptr<std::thread> m_send_thread;
    bool m_connection_established = false;
    std::shared_ptr<Allxon::JsonValidator> m_json_validator;

public:
    WebSocketClient(std::shared_ptr<Allxon::JsonValidator> json_validator): m_json_validator(json_validator) 
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

            if (++count == 60)
            {
                SendPluginStatesMetrics();
                count = 0;
            }
        }
    }
};

using namespace Allxon;

int main(int argc, char **argv)
{
    Log start; // start Logging
    UTL_LOG_INFO("BUILD_INFO: %s", BUILD_INFO);

    // if (!getLock()) // Check single instance app
    // {
    //     UTL_LOG_WARN("Process already running!\n", stderr);
    //     return 1;
    // }

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
    // argv[1];
    auto json_source_string = getJsonFromFile(argv[1]);
    auto json_validator = std::make_shared<JsonValidator>(CMAKE_PROJECT_NAME, PLUGIN_APP_GUID,
                                                          PLUGIN_ACCESS_KEY, CMAKE_PROJECT_VERSION, json_source_string);
    WebSocketClient web_client(json_validator);
    web_client.Connect("wss://127.0.0.1:55688");
    web_client.RunSendingLoop();
    std::cout << "here" << std::endl;
    return 0;
}
