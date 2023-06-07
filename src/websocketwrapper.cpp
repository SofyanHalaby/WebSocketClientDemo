#include "websocketwrapper.hpp"
#include <string>
#define CLOSE_NORMAL 1000

ConnectionMetaData::ConnectionMetaData(int id, client &endpoint, websocketpp::connection_hdl hdl, std::shared_ptr<Subscribtion> subscribtion, std::shared_ptr<EventHandler> event_handler)
    : m_id(id), m_endpoint(endpoint), m_hdl(hdl), m_subscribtion(subscribtion), m_event_handler(event_handler), m_status("openning")
{
}

void ConnectionMetaData::OnOpen(client *c, websocketpp::connection_hdl hdl)
{
    m_event_handler->HandleLogs("[INFO] connection [" + std::to_string(m_id) + "] opened");
    m_status = "open";
    websocketpp::lib::error_code ec;
    m_endpoint.send(m_hdl, m_subscribtion->GenerateRequest(), websocketpp::frame::opcode::text, ec);
    if (ec)
    {
        m_event_handler->HandleLogs("[ERROR] connection [" + std::to_string(m_id) + "] cannot send subscription request, error message: " + ec.message());
    }
    else
    {
        m_event_handler->HandleLogs("[INFO] connection [" + std::to_string(m_id) + "] subscription request is successfully sent");
    }
}

void ConnectionMetaData::OnFail(client *c, websocketpp::connection_hdl hdl)
{
    m_status = "failed";
    m_event_handler->HandleLogs("[ERROR] connection [" + std::to_string(m_id) + "] cannot be opened");
    //should remove from the endpoint
}

void ConnectionMetaData::OnClose(client *c, websocketpp::connection_hdl hdl)
{
    m_status = "closed";
    m_event_handler->HandleLogs("[INFO] connection [" + std::to_string(m_id) + "] closed correctly");
}

void ConnectionMetaData::OnMessageReceived(websocketpp::connection_hdl hdl, client::message_ptr msg)
{
    if (m_status != "open")
    {
        return;
    }
    m_event_handler->HandleLogs("[INFO] connection [" + std::to_string(m_id) + "] message received");
    m_event_handler->HandleNewMessage(msg->get_payload());
}

void ConnectionMetaData::Suspend()
{
    m_status = "suspended";
}

std::shared_ptr<Subscribtion> ConnectionMetaData::GetSubscribtion()
{
    return m_subscribtion;
}

websocketpp::connection_hdl ConnectionMetaData::GetHandle()
{
    return m_hdl;
}

std::string ConnectionMetaData::GetStatus()
{
    return m_status;
}

WebSocketEndPoint::WebSocketEndPoint(std::string uri, std::shared_ptr<EventHandler> event_handler)
    : m_next_id(1), m_event_handler(event_handler), m_uri(uri)
{
    m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
    m_endpoint.clear_error_channels(websocketpp::log::elevel::all);

    m_endpoint.init_asio();
    m_endpoint.set_tls_init_handler(std::bind(&WebSocketEndPoint::on_tls_init));
    m_endpoint.start_perpetual();

    m_thread.reset(new websocketpp::lib::thread(&client::run, &m_endpoint));
}

WebSocketEndPoint::~WebSocketEndPoint()
{
    m_endpoint.stop_perpetual();
    for (con_list::const_iterator it = m_connection_list.begin(); it != m_connection_list.end(); ++it)
    {
        if (it->second->GetStatus() != "open")
        {
            // Only close open connections
            continue;
        }

        std::cout << "> Closing connection " << it->first << std::endl;

        websocketpp::lib::error_code ec;
        m_endpoint.close(it->second->GetHandle(), websocketpp::close::status::going_away, "", ec);
        if (ec)
        {
            std::cout << "> Error closing connection " << it->first << ": "
                      << ec.message() << std::endl;
        }
    }

    m_thread->join();
}

websocketpp::lib::shared_ptr<WebSocketEndPoint::sslcontext> WebSocketEndPoint::on_tls_init()
{
    // establishes a SSL connection
    std::shared_ptr<WebSocketEndPoint::sslcontext> ctx = std::make_shared<WebSocketEndPoint::sslcontext>(WebSocketEndPoint::sslcontext::sslv23);

    boost::system::error_code ec;
    ctx->set_options(websocketpp::lib::asio::ssl::context::default_workarounds |
                         websocketpp::lib::asio::ssl::context::no_sslv2 |
                         websocketpp::lib::asio::ssl::context::no_sslv3 |
                         websocketpp::lib::asio::ssl::context::single_dh_use,
                     ec);
    // if (ec)
    // {
    //     //m_event_handler->HandleLogs("[ERROR] Error initializing ssl: " + ec.message());
    // }

    return ctx;
}

int WebSocketEndPoint::Subscribe(websocketpp::lib::shared_ptr<Subscribtion> subscribtion)
{
    websocketpp::lib::error_code ec;

    client::connection_ptr con = m_endpoint.get_connection(m_uri, ec);

    if (ec)
    {
        m_event_handler->HandleLogs("[ERROR] Connect initialization error:" + ec.message());
        return -1;
    }

    int new_id = m_next_id++;
    std::shared_ptr<ConnectionMetaData> connection_details(new ConnectionMetaData(new_id, m_endpoint, con->get_handle(), subscribtion, m_event_handler));
    m_connection_list[new_id] = connection_details;

    con->set_open_handler(websocketpp::lib::bind(
        &ConnectionMetaData::OnOpen,
        connection_details,
        &m_endpoint,
        websocketpp::lib::placeholders::_1));
    con->set_fail_handler(websocketpp::lib::bind(
        &ConnectionMetaData::OnFail,
        connection_details,
        &m_endpoint,
        websocketpp::lib::placeholders::_1));
    con->set_message_handler(websocketpp::lib::bind(
        &ConnectionMetaData::OnMessageReceived,
        connection_details,
        websocketpp::lib::placeholders::_1,
        websocketpp::lib::placeholders::_2));

    m_endpoint.connect(con);

    return new_id;
}

bool WebSocketEndPoint::Disconnect(int id)
{
    websocketpp::lib::error_code ec;
    con_list::iterator metadata_it = m_connection_list.find(id);
    if (metadata_it == m_connection_list.end())
    {
        m_event_handler->HandleLogs("[ERROR] no connection found with id " + id);
        return false;
    }
    metadata_it->second->Suspend();
    m_endpoint.close(metadata_it->second->GetHandle(), CLOSE_NORMAL, "", ec);
    if (ec)
    {
        m_event_handler->HandleLogs("[ERROR] error occured while closing connection [" + std::to_string(id) + "]: " + ec.message());
        return false;
    }
    m_connection_list.erase(metadata_it);
    return true;
}

const WebSocketEndPoint::con_list WebSocketEndPoint::GetConnections() const
{
    return m_connection_list;
}
