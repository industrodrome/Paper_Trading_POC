//Contains Deribit Class
//Connection to Deribit via API sends requests using HTTP

/**

#include <iostream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

class DeribitAPI
{
private:

    std::string access_token = "";

    // Function to handle the response from the cURL request
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) 
    {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    // General function to send a cURL request with optional access token
    std::string sendRequest(const std::string& url, const nlohmann::json& payload) 
    {
        std::string read_buffer;
        CURL* curl;
        CURLcode res;
        // Set headers, including Authorization if access token is provided
        struct curl_slist* headers = NULL;

        try {
            // Initialize cURL globally
            curl_global_init(CURL_GLOBAL_DEFAULT);
            curl = curl_easy_init();

            if (!curl) {
                throw std::runtime_error("Failed to initialize cURL.");
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);  // Set the HTTP method to POST

            // Set the request payload
            std::string json_str = payload.dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());


            headers = curl_slist_append(headers, "Content-Type: application/json");
            if (!access_token.empty()) {
                headers = curl_slist_append(headers, ("Authorization: Bearer " + access_token).c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            // Set up the write callback to capture the response
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);

            // Perform the request
            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                throw std::runtime_error("Request failed: " + std::string(curl_easy_strerror(res)));
            }

            // Free Resources
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            curl_global_cleanup();
            return read_buffer;

        }
        catch (const std::exception& e) {
            // Catch any standard exceptions and print the error message
            std::cerr << "Error: " << e.what() << std::endl;

            // Ensure to clean up resources even if an exception occurs
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            curl_global_cleanup();

            // Optionally, rethrow or return an empty string to indicate failure
            return "";
        }
    }



public:
    // Authenticate and Get Access Token

    bool authenticate(const std::string& client_id, const std::string& client_secret) {
        try {
            // Construct the payload for authentication
            nlohmann::json payload = {
                {"id", 0},
                {"method", "public/auth"},
                {"params", {
                    {"grant_type", "client_credentials"},
                    {"scope", "session:apiconsole-c5i26ds6dsr expires:2592000"},
                    {"client_id", client_id},
                    {"client_secret", client_secret}
                }},
                {"jsonrpc", "2.0"}
            };

            // Send the request and get the response
            std::string response = sendRequest("https://test.deribit.com/api/v2/public/auth", payload);

            // Parse the JSON response
            auto response_json = nlohmann::json::parse(response);

            // Retrieve the access token from the response
            if (response_json.contains("result") && response_json["result"].contains("access_token")) {
                // Store access token
                access_token = response_json["result"]["access_token"];

                // Signal Authentication success
                return true;
            }
            else {
                std::cerr << "Failed to retrieve access token." << std::endl;
                return false;
            }

        }
        catch (const std::exception& e) {
            // Catch any exceptions (e.g., JSON parsing errors, network errors)
            std::cerr << "Error during authentication: " << e.what() << std::endl;
            return false;
        }
    }


    
    // Place Order
    std::string placeOrder(const float& price, const int& qty, const std::string& instrument, const int& buy_or_sell) {
        try {
            std::string method;
            nlohmann::json response;
            std::string order_id;

            // Determine the method based on buy_or_sell
            if (buy_or_sell == 1) {
                method = "private/buy";
            }
            else {
                method = "private/sell";
            }

            // Create the payload for the request
            nlohmann::json payload = {
                {"jsonrpc", "2.0"},
                {"method", method},
                {"params", {
                    {"instrument_name", instrument},
                    {"type", "limit"},
                    {"price", price},
                    {"amount", qty}
                }},
                {"id", 1}
            };

            // Construct the URL for the request
            std::string url = "https://test.deribit.com/api/v2/" + method;

            // Send the request and get the raw response
            std::string raw_response = sendRequest(url, payload);

            // Parse the Raw Response into JSON to extract Order ID
            response = nlohmann::json::parse(raw_response);

            // Check if the response contains the expected data
            if (response.is_object() && response.contains("result") && response["result"].contains("order")) {
                std::cout << "Order Placed" << std::endl;
                order_id = response["result"]["order"]["order_id"];
                std::cout << "Order ID = " << order_id << std::endl;
            }
            else {
                std::cout << "Error: Could not extract order ID from the response." << std::endl;
            }

            // Return the Order ID
            return order_id;

        }
        catch (const std::exception& e) {
            // Catch any exceptions (e.g., JSON parsing errors, network errors)
            std::cerr << "Error during order placement: " << e.what() << std::endl;
            return ""; // Return empty string in case of failure
        }
    }


    // Function to cancel an order
    void cancelOrder(const std::string& order_id)
    {
        try {
            // Create the payload for the request
            nlohmann::json payload = {
                {"jsonrpc", "2.0"},
                {"method", "private/cancel"},
                {"params", {{"order_id", order_id}}},
                {"id", 6}
            };

            nlohmann::json response;

            // Send the request and get the raw response
            std::string raw_response = sendRequest("https://test.deribit.com/api/v2/private/cancel", payload);

            // Parse the raw response into JSON
            response = nlohmann::json::parse(raw_response);

            //std::cout << "Cancel Order Response: " << response << std::endl;

            // Check if the response contains the expected data
            if (response.is_object() && response.contains("result")) {
                std::cout << "Order Cancelled Successfully!" << std::endl;
            }
            else {
                std::cout << "Error: Could not cancel the order." << std::endl;
            }

        }
        catch (const std::exception& e) {
            // Catch any exceptions (e.g., network errors, JSON parsing errors)
            std::cerr << "Error during order cancellation: " << e.what() << std::endl;
        }
    }


    // Function to modify an order
    void modifyOrder(const std::string& order_id, int amount, double price) {
        try {
            // Create the payload for the request
            nlohmann::json payload = {
                {"jsonrpc", "2.0"},
                {"method", "private/edit"},
                {"params", {
                    {"order_id", order_id},
                    {"amount", amount},
                    {"price", price}
                }},
                {"id", 11}
            };

            // Send the request and get the raw response
            std::string raw_response = sendRequest("https://test.deribit.com/api/v2/private/edit", payload);

            // Parse the raw response into JSON
            nlohmann::json response = nlohmann::json::parse(raw_response);

            // Check if the response contains the expected data
            if (response.is_object()) {
                std::cout << "Order Modified Successfully!" << std::endl;
            }
            else {
                std::cout << "Unable to Modify Order" << std::endl;
            }

        }
        catch (const std::exception& e) {
            // Catch any exceptions (e.g., network errors, JSON parsing errors)
            std::cerr << "Error during order modification: " << e.what() << std::endl;
        }
    }



    // Function to retrieve the order book
    void getOrderBook(const std::string& instrument , const int& depth) {
        try {
            // Create the payload for the request
            nlohmann::json payload = {
                                    {"jsonrpc", "2.0"},
                                    {"method", "public/get_order_book"},
                                    {"params", {
                                        {"instrument_name", instrument},
                                        {"depth", depth}
                                    }},
                                    {"id", 15}
                                       };

            // Send the request and get the raw response
            std::string response = sendRequest("https://test.deribit.com/api/v2/public/get_order_book", payload);

            // Parse the raw response into JSON
            nlohmann::json response_json = nlohmann::json::parse(response);

            // Check if the response contains the expected data
            std::cout << "Order Book for " << instrument << ":\n\n";

            // Printing best bid and ask
            std::cout << "Best Bid Price: " << response_json["result"]["best_bid_price"] << ", Amount: " << response_json["result"]["best_bid_amount"] << '\n';
            std::cout << "Best Ask Price: " << response_json["result"]["best_ask_price"] << ", Amount: " << response_json["result"]["best_ask_amount"] << '\n';

            // Printing bids and asks in detail
            std::cout << "Asks:\n";
            for (const auto& ask : response_json["result"]["asks"]) {
                std::cout << "Price: " << ask[0] << ", Amount: " << ask[1] << '\n';
            }

            std::cout << "\nBids:\n";
            for (const auto& bid : response_json["result"]["bids"]) {
                std::cout << "Price: " << bid[0] << ", Amount: " << bid[1] << '\n';
            }

            // Additional information
            std::cout << "\nMark Price: " << response_json["result"]["mark_price"] << '\n';
            std::cout << "Open Interest: " << response_json["result"]["open_interest"] << '\n';
            std::cout << "Timestamp: " << response_json["result"]["timestamp"] << '\n';

        }
        catch (const std::exception& e) {
            // Catch any exceptions (e.g., network errors, JSON parsing errors)
            std::cerr << "Error retrieving order book: " << e.what() << std::endl;
        }
    }



    // Function to get position details of a specific instrument
    void getPosition(const std::string& instrument) {
        try {
            // Create the payload for the request
            nlohmann::json payload = {
                {"jsonrpc", "2.0"},
                {"method", "private/get_position"},
                {"params", {{"instrument_name", instrument}}},
                {"id", 20}
            };

            // Send the request and get the raw response
            std::string response = sendRequest("https://test.deribit.com/api/v2/private/get_position", payload);

            // Parse the raw response into JSON
            nlohmann::json response_json = nlohmann::json::parse(response);

            // Check if the response contains the expected data
            if (response_json.contains("result")) {
                std::cout << "Position Details for " << instrument << ":\n\n";

                auto result = response_json["result"];
                std::cout << "Estimated Liquidation Price: " << result["estimated_liquidation_price"] << '\n';
                std::cout << "Size Currency: " << result["size_currency"] << '\n';
                std::cout << "Realized Funding: " << result["realized_funding"] << '\n';
                std::cout << "Total Profit Loss: " << result["total_profit_loss"] << '\n';
                std::cout << "Realized Profit Loss: " << result["realized_profit_loss"] << '\n';
                std::cout << "Floating Profit Loss: " << result["floating_profit_loss"] << '\n';
                std::cout << "Leverage: " << result["leverage"] << '\n';
                std::cout << "Average Price: " << result["average_price"] << '\n';
                std::cout << "Delta: " << result["delta"] << '\n';
                std::cout << "Interest Value: " << result["interest_value"] << '\n';
                std::cout << "Mark Price: " << result["mark_price"] << '\n';
                std::cout << "Settlement Price: " << result["settlement_price"] << '\n';
                std::cout << "Index Price: " << result["index_price"] << '\n';
                std::cout << "Direction: " << result["direction"] << '\n';
                std::cout << "Open Orders Margin: " << result["open_orders_margin"] << '\n';
                std::cout << "Initial Margin: " << result["initial_margin"] << '\n';
                std::cout << "Maintenance Margin: " << result["maintenance_margin"] << '\n';
                std::cout << "Kind: " << result["kind"] << '\n';
                std::cout << "Size: " << result["size"] << '\n';
            }
            else {
                std::cerr << "Error: Could not retrieve position data." << std::endl;
            }
        }
        catch (const std::exception& e) {
            // Catch any exceptions (e.g., network errors, JSON parsing errors)
            std::cerr << "Error retrieving position data: " << e.what() << std::endl;
        }
    }



    // Function to print all open orders with instrument, order ID, price, and quantity
    void getOpenOrders() {
        try {
            // Create the payload for the request
            nlohmann::json payload = {
                {"jsonrpc", "2.0"},
                {"method", "private/get_open_orders"},
                {"params", {{"kind", "future"}, {"type", "limit"}}},
                {"id", 25}
            };

            // Send the request and get the raw response
            std::string response = sendRequest("https://test.deribit.com/api/v2/private/get_open_orders", payload);

            // Parse the raw response into JSON
            nlohmann::json response_json = nlohmann::json::parse(response);

            // Check if the response contains the "result" array
            if (response_json.contains("result")) {
                std::cout << "Open Orders:\n\n";
                for (const auto& order : response_json["result"]) {
                    std::string instrument = order["instrument_name"];
                    std::string orderId = order["order_id"];
                    double price = order["price"];
                    double amount = order["amount"];

                    std::cout << "Instrument: " << instrument << ", Order ID: " << orderId
                        << ", Price: " << price << ", Amount: " << amount << '\n';
                }
            }
            else {
                std::cerr << "Error: Could not retrieve open orders." << std::endl;
            }
        }
        catch (const std::exception& e) {
            // Catch any exceptions (e.g., network errors, JSON parsing errors)
            std::cerr << "Error retrieving open orders: " << e.what() << std::endl;
        }
    }

};
int main() {
    
    //Variables to store id and secret
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


    //Define Empty Variables
    std::string order_id;
    int qty;
    float price;
    std::string instrument;

    //Create object for Deribit API
    DeribitAPI deribit;

    // Retrieve the access token
    bool isAuthenticated = deribit.authenticate(client_id, client_secret);

    
        // Check if authenticated
        if (!isAuthenticated)
        {
            throw std::runtime_error("Authentication failed: Unable to retrieve the access token.");
        }


        while (true)
        {
            std::cout << "Enter your choice :" << std::endl;
            std::cout << "\n 1) Place Order \n 2) Cancel Order \n 3) Modify Order \n 4) View OrderBook \n 5) View Positions \n 6) View Open Orders \n 7) Exit Trading" << std::endl;
            int choice;
            std::cin >> choice;

            if (choice == 7)
                break;

            switch (choice)
            {
            case 1: // Place Buy Orders
            {


                int buy_or_sell;

                std::cout << "Enter 1 for BUY | 2 for SELL" << std::endl;
                std::cin >> buy_or_sell;

                std::cout << "Enter Instrument :" << std::endl;
                std::cin >> instrument;
                std::cout << " Enter Price :" << std::endl;
                std::cin >> price;
                std::cout << "Enter Quantity :" << std::endl;
                std::cin >> qty;

                order_id = deribit.placeOrder(price, qty, instrument, buy_or_sell);
                break;

            }
            case 2: // Cancel Orders
            {

                std::cout << "Enter Order Id :" << std::endl;
                std::cin >> order_id;
                deribit.cancelOrder(order_id);

                std::cout << " Order Id : " << order_id << " -- CANCELLED" << std::endl;
                break;

            }
            case 3: // Modify Orders
            {
                std::cout << "Enter Order ID : " << std::endl;
                std::cin >> order_id;
                std::cout << "Enter Quantity : " << std::endl;
                std::cin >> qty;
                std::cout << "Enter Price : " << std::endl;
                std::cin >> price;

                deribit.modifyOrder(order_id, qty, price);
                std::cout << " Modified Order : Order ID = " << order_id << " Price : " << price << " Quantity " << qty << std::endl;
                break;
            }
            case 4: // Get Orderbook
            {

                int depth;
                std::cout << "Enter Instrument :" << std::endl;
                std::cin >> instrument;

                std::cout << "Enter Depth :" << std::endl;
                std::cin >> depth;
                deribit.getOrderBook(instrument , depth);
                break;
            }
            case 5: //View Positions
            {

                std::cout << "Enter Instrument :" << std::endl;
                std::cin >> instrument;
                deribit.getPosition(instrument);
                break;
            }
            case 6: //View Open Orders
            {
                deribit.getOpenOrders();
                break;
            }

            default:
                std::cout << "Wrong Input" << std::endl;
            }
        }
 
    return 0;
}

**/