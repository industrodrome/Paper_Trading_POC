
#include <fstream>
#include <sstream>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <thread>
#include<chrono>

using json = nlohmann::json;

class DeribitWebSocket {
private:
    typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
    typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;

    client m_client;
    websocketpp::connection_hdl m_hdl;
    std::string access_token;
    std::string client_id;
    std::string client_secret;
    bool connection_success = false;

    static context_ptr on_tls_init(websocketpp::connection_hdl) {
        context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
        ctx->set_verify_mode(boost::asio::ssl::verify_none); // Disable SSL verification
        return ctx;
    }

    // Function to open connection
    void on_open(websocketpp::connection_hdl hdl) {
        //std::cout << "[DEBUG] : inside on_open" << std::endl;
        m_hdl = hdl;
        // Send authentication request after connection is opened
        json auth_request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "public/auth"},
            {"params", {
                {"grant_type", "client_credentials"},
                {"client_id", client_id},
                {"client_secret", client_secret}
            }}
        };
        m_client.send(m_hdl, auth_request.dump(), websocketpp::frame::opcode::text);
    }

    // Function to read message
    void on_message(websocketpp::connection_hdl, client::message_ptr msg) {
        //std::cout << "[DEBUG] : inside on_message" << std::endl;
        json response;
        try {
            response = json::parse(msg->get_payload());

            // Check for access token in the authentication response
            if (response.contains("result") && response["result"].contains("access_token")) {
                access_token = response["result"]["access_token"].get<std::string>();
                connection_success = true;
                //std::cout << "Connection: Success" << std::endl;


                // Subscribe after authentication is successful
                subscribe("deribit_price_index.btc_usd");

            }
            // Handle Server Response
            else if (response.contains("params")) {
                if (response["params"].contains("data")) {
                    json order_book_data = response["params"]["data"];
                    std::cout << "Server Response : " << order_book_data.dump(4) << std::endl;
                }
            }
            else {
                std::cerr << "Message received: " << msg->get_payload() << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error parsing message: " << e.what() << std::endl;
        }
    }

    // Function if connection fails
    void on_fail(websocketpp::connection_hdl) {
        //std::cout << "[DEBUG] : inside on_fail" << std::endl;
        std::cerr << "Connection failed." << std::endl;
    }

    // Function if connection closes
    void on_close(websocketpp::connection_hdl) {
        //std::cout << "[DEBUG] : inside on_close" << std::endl;
        std::cout << "Connection closed." << std::endl;
    }

    // Function to subscribe
    void subscribe(const std::string& symbol) {
        //std::cout << "[DEBUG] : inside void subscribe" << std::endl;
        json subscribe_request = {
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "public/subscribe"},
            {"params", {
                {"channels", {symbol}}
            }}
        };

        m_client.send(m_hdl, subscribe_request.dump(), websocketpp::frame::opcode::text);
    }

    // Function to unsubscribe
    void unsubscribe(const std::string& symbol)
    {
        //std::cout << "[DEBUG] : inside void unsubscribe" << std::endl;
        json unsubscribe_request = {
            {"jsonrpc", 2.0},
            {"id" , 3},
            {"method", "public/unsubscribe"},
            {"params", {
                {"channels", {symbol}}
            }}
        };

        m_client.send(m_hdl, unsubscribe_request.dump(), websocketpp::frame::opcode::text);
    }


public:
    // Derbit constructor for initialisation
    DeribitWebSocket(const std::string& client_id, const std::string& client_secret)
        : client_id(client_id), client_secret(client_secret)
    {
        m_client.init_asio();
        m_client.set_tls_init_handler(std::function<context_ptr(websocketpp::connection_hdl)>(DeribitWebSocket::on_tls_init));
        m_client.set_open_handler(websocketpp::lib::bind(&DeribitWebSocket::on_open, this, websocketpp::lib::placeholders::_1));
        m_client.set_message_handler(websocketpp::lib::bind(&DeribitWebSocket::on_message, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
        m_client.set_fail_handler(websocketpp::lib::bind(&DeribitWebSocket::on_fail, this, websocketpp::lib::placeholders::_1));
        m_client.set_close_handler(websocketpp::lib::bind(&DeribitWebSocket::on_close, this, websocketpp::lib::placeholders::_1));
    }

    // Function to establish connection
    void connect(const std::string& uri) {
        //std::cout << "[DEBUG] : inside void connect" << std::endl;
        websocketpp::lib::error_code ec;
        client::connection_ptr con = m_client.get_connection(uri, ec);
        if (ec) {
            std::cerr << "Connection error: " << ec.message() << std::endl;
            return;
        }

        m_client.connect(con);

        // Run the client in a separate thread
        std::thread(&client::run, &m_client).detach();
    }

    // Function to disconnect connection
    void disconnect() {
        //std::cout << "[DEBUG] : Inside disconnect" << std::endl;
        m_client.close(m_hdl, websocketpp::close::status::normal, "Streaming completed");
    }

    // Function to signal connection success/failure
    bool is_connected() const {
        return connection_success;
    }
};



int main() {

    // Provide client ID and secret here
    std::string client_id;
    std::string client_secret;

    // File to read credentials from
    std::ifstream inputFile("credentials.txt");

    try {
        // Check if the file was successfully opened
        if (!inputFile.is_open()) {
            throw std::ios_base::failure("Error opening credentials file!");
        }

        // Parse the JSON file
        json credentials;
        inputFile >> credentials;

        // Extract and store client_id and client_secret
        client_id = credentials.at("client_id");
        client_secret = credentials.at("client_secret");
    }
    catch (const std::ios_base::failure& e) {
        std::cerr << "File I/O Error: " << e.what() << std::endl;
        return 1;
    }
    catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        return 1;
    }


    // Initialise the Websocket with client id and client secret
    DeribitWebSocket ws(client_id, client_secret);

    // Replace with the actual WebSocket URI for Deribit test environment
    std::string uri = "wss://test.deribit.com/ws/api/v2";

    std::cout << "Press any key to subscribe to : deribit_price_index.btc_usd" << std::endl;
    std::string input;
    std::cin >> input;

    ws.connect(uri);

    std::this_thread::sleep_for(std::chrono::seconds(60));

    if (ws.is_connected()) {
        //std::cout << "WebSocket connection and authentication successful." << std::endl;

        // Stream data for the specified duration
        //std::this_thread::sleep_for(std::chrono::seconds(duration));

        // Disconnect after the duration
        //ws.disconnect();
        //std::cout << "Streaming completed. WebSocket disconnected." << std::endl;

    }
    else {
        std::cerr << "WebSocket connection or authentication failed." << std::endl;
    }

    // Keep the main thread alive while WebSocket runs in the background
    std::this_thread::sleep_for(std::chrono::seconds(60));

    return 0;
}
