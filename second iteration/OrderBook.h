#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include<sstream>




class OrderBook {
private:
    struct Order {
        int id;
        int clientId; // Unique ID for the client
        std::string side; // "buy" or "sell"
        double price;
        int quantity;

        // Comparator for buy orders (max-heap for highest price priority)
        bool operator<(const Order& other) const {
            if (side == "buy") {
                return price < other.price; // Higher prices have priority
            }
            return price > other.price; // Lower prices have priority for sell orders
        }
    };

    std::priority_queue<Order> buyOrders;
    std::priority_queue<Order> sellOrders;
    std::unordered_map<int, std::vector<Order>> clientOrders;  // Store client orders
    std::unordered_map<int, std::vector<Order>> executedOrders;  // Store executed orders

    int orderIdCounter;
    std::mutex orderBookMutex;
    std::condition_variable cv;

public:
    OrderBook() : orderIdCounter(0) {}


    void addOrder(int clientId, const std::string& side, double price, int quantity) {
        std::lock_guard<std::mutex> lock(orderBookMutex);

        Order order = { ++orderIdCounter, clientId, side, price, quantity };
        clientOrders[clientId].push_back(order);

        if (side == "buy") {
            buyOrders.push(order);
        }
        else if (side == "sell") {
            sellOrders.push(order);
        }

        std::cout << "Order added: ClientID=" << clientId
            << ", ID=" << order.id
            << ", Side=" << side
            << ", Price=" << price
            << ", Quantity=" << quantity << std::endl;

        cv.notify_one();
    }


    /**
    // Add a new order
    void addOrder(int clientId, const std::string& side, double price, int quantity) {
        std::lock_guard<std::mutex> lock(orderBookMutex);

        Order order = { ++orderIdCounter, clientId, side, price, quantity };

        if (side == "buy") {
            buyOrders.push(order);
        }
        else if (side == "sell") {
            sellOrders.push(order);
        }

        std::cout << "Order added: ClientID=" << clientId
            << ", ID=" << order.id
            << ", Side=" << side
            << ", Price=" << price
            << ", Quantity=" << quantity << std::endl;

        cv.notify_one();
    }

    // Match buy and sell orders
    
    void matchOrders() {
        std::unique_lock<std::mutex> lock(orderBookMutex);

        while (true) {
            cv.wait(lock, [this]() {
                return !buyOrders.empty() && !sellOrders.empty();
                });

            while (!buyOrders.empty() && !sellOrders.empty()) {
                Order buy = buyOrders.top();
                Order sell = sellOrders.top();

                if (buy.price >= sell.price) {
                    int tradedQuantity = std::min(buy.quantity, sell.quantity);

                    std::cout << "Trade executed: BuyID=" << buy.id
                        << ", SellID=" << sell.id
                        << ", Price=" << sell.price
                        << ", Quantity=" << tradedQuantity << std::endl;

                    buy.quantity -= tradedQuantity;
                    sell.quantity -= tradedQuantity;

                    if (buy.quantity == 0) buyOrders.pop();
                    else {
                        buyOrders.pop();
                        buyOrders.push(buy);
                    }

                    if (sell.quantity == 0) sellOrders.pop();
                    else {
                        sellOrders.pop();
                        sellOrders.push(sell);
                    }
                }
                else {
                    break; // No more matching orders
                }
            }
        }
    }
    **/



    void matchOrders() {
        std::unique_lock<std::mutex> lock(orderBookMutex);

        while (true) {
            cv.wait(lock, [this]() {
                return !buyOrders.empty() && !sellOrders.empty();
                });

            while (!buyOrders.empty() && !sellOrders.empty()) {
                Order buy = buyOrders.top();
                Order sell = sellOrders.top();

                if (buy.price >= sell.price) {
                    int tradedQuantity = std::min(buy.quantity, sell.quantity);

                    std::cout << "Trade executed: BuyID=" << buy.id
                        << ", SellID=" << sell.id
                        << ", Price=" << sell.price
                        << ", Quantity=" << tradedQuantity << std::endl;

                    executedOrders[buy.clientId].push_back({ buy.id, buy.clientId, "buy", buy.price, tradedQuantity });
                    executedOrders[sell.clientId].push_back({ sell.id, sell.clientId, "sell", sell.price, tradedQuantity });

                    buy.quantity -= tradedQuantity;
                    sell.quantity -= tradedQuantity;

                    if (buy.quantity == 0) buyOrders.pop();
                    else {
                        buyOrders.pop();
                        buyOrders.push(buy);
                    }

                    if (sell.quantity == 0) sellOrders.pop();
                    else {
                        sellOrders.pop();
                        sellOrders.push(sell);
                    }
                }
                else {
                    break;  // No more matching orders
                }
            }
        }
    }


    std::string getOrderBookForClient(int clientId) {
        std::lock_guard<std::mutex> lock(orderBookMutex);
        std::ostringstream oss;

        oss << "Your Active Orders:\n";

        std::priority_queue<Order> tempBuy = buyOrders;
        while (!tempBuy.empty()) {
            Order order = tempBuy.top();
            tempBuy.pop();
            if (order.clientId == clientId) {
                oss << "[BUY] Price: " << order.price << " Qty: " << order.quantity << "\n";
            }
        }

        std::priority_queue<Order> tempSell = sellOrders;
        while (!tempSell.empty()) {
            Order order = tempSell.top();
            tempSell.pop();
            if (order.clientId == clientId) {
                oss << "[SELL] Price: " << order.price << " Qty: " << order.quantity << "\n";
            }
        }

        return oss.str();
    }







    // Display the current order book
    void displayOrderBook() {
        std::lock_guard<std::mutex> lock(orderBookMutex);

        std::cout << "Buy Orders:" << std::endl;
        std::priority_queue<Order> tempBuy = buyOrders;
        while (!tempBuy.empty()) {
            Order order = tempBuy.top();
            tempBuy.pop();
            std::cout << "  ClientID=" << order.clientId
                << ", ID=" << order.id
                << ", Price=" << order.price
                << ", Quantity=" << order.quantity << std::endl;
        }

        std::cout << "Sell Orders:" << std::endl;
        std::priority_queue<Order> tempSell = sellOrders;
        while (!tempSell.empty()) {
            Order order = tempSell.top();
            tempSell.pop();
            std::cout << "  ClientID=" << order.clientId
                << ", ID=" << order.id
                << ", Price=" << order.price
                << ", Quantity=" << order.quantity << std::endl;
        }
    }
};

#endif // ORDERBOOK_H