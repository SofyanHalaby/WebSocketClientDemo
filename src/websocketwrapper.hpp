#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/asio.hpp>
#include <map>
#include <string>
#include<thread>
#include<memory>

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;

class EventHandler
{
public:
    virtual void HandleNewMessage(std::string message) = 0;
    virtual void HandleLogs(std::string log) = 0;
};

class Subscribtion
{
public:
    virtual std::string GenerateRequest() = 0;
};

class ConnectionMetaData
{
public:
    ConnectionMetaData(int id, client& endpoint, websocketpp::connection_hdl hdl, std::shared_ptr<Subscribtion> subscribtion, std::shared_ptr<EventHandler> event_handler);
    void OnOpen(client *c, websocketpp::connection_hdl hdl);
    void OnFail(client *c, websocketpp::connection_hdl hdl);
    void OnClose(client *c, websocketpp::connection_hdl hdl);
    void OnMessageReceived(websocketpp::connection_hdl hdl, client::message_ptr msg);
    void Suspend();
    std::shared_ptr<Subscribtion> GetSubscribtion();
    websocketpp::connection_hdl GetHandle();
    std::string GetStatus();

private:
    std::string m_status;
    websocketpp::connection_hdl m_hdl;
    client& m_endpoint;

    int m_id;
    std::shared_ptr<Subscribtion> m_subscribtion;
    std::shared_ptr<EventHandler> m_event_handler;
};


class WebSocketEndPoint
{
private:
    typedef websocketpp::lib::asio::ssl::context sslcontext;
    typedef std::map<int, std::shared_ptr<ConnectionMetaData>> con_list;
public:
    WebSocketEndPoint(std::string uri, std::shared_ptr<EventHandler> event_handler);
    ~WebSocketEndPoint();
    static std::shared_ptr<sslcontext> on_tls_init();
    int Subscribe(std::shared_ptr<Subscribtion> subscribtion);
    bool Disconnect(int id);
    const con_list GetConnections() const;

private:
    
    std::shared_ptr<EventHandler> m_event_handler;
    client m_endpoint;
    std::shared_ptr<std::thread> m_thread;
    std::string m_uri;
    con_list m_connection_list;
    int m_next_id;
};
