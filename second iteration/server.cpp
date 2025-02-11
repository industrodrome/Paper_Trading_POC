//server

#include "OrderBook.h"
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include<atomic>

#define PORT 8080
#define BUFFER_SIZE 1024

std::mutex console_mutex;
std::atomic<int> client_id_counter(1); // Atomic counter for unique client IDs
OrderBook orderBook;

// Struct to store orders
struct Order {
    char side[5];  // Buy or Sell
    char type[10]; // Limit or Market
    int qty;
    double price;
};

// Split string input into Order struct
Order split_input(const std::string& input , int client_id) {
    Order order;
    std::vector<std::string> components;
    std::stringstream ss(input);
    std::string segment;

    while (std::getline(ss, segment, '|')) {
        components.push_back(segment);
    }

    if (components.size() == 4) {
        std::strncpy(order.type, components[0].c_str(), sizeof(order.type) - 1);
        order.type[sizeof(order.type) - 1] = '\0';

        std::strncpy(order.side, components[1].c_str(), sizeof(order.side) - 1);
        order.side[sizeof(order.side) - 1] = '\0';

        order.qty = std::stoi(components[2]);
        order.price = std::stod(components[3]);

        // here
        orderBook.addOrder(client_id, order.side, order.price, order.qty);
    }
    else {
        std::cerr << "Invalid message format: " << input << std::endl;
    }

    return order;
}


void handleClient(int client_socket, int client_id) {
    char buffer[BUFFER_SIZE];

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);

        if (bytes_read <= 0) {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "Client " << client_id << " disconnected." << std::endl;
            close(client_socket);
            break;
        }

        std::string message(buffer);
        Order order = split_input(message, client_id);

        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "Message from client " << client_id << " : " << message << std::endl;

        // Send order confirmation
        std::string response = "Order Confirmed : [" +
            std::string(order.type) + " | " +
            std::string(order.side) + " | " +
            std::to_string(order.qty) + " | " +
            std::to_string(order.price) + "]\n";
        send(client_socket, response.c_str(), response.length(), 0);

        // Send the updated order book to the client
        std::string orderBookInfo = orderBook.getOrderBookForClient(client_id);
        send(client_socket, orderBookInfo.c_str(), orderBookInfo.length(), 0);
    }
}





/**
// Function to handle client communication
void handleClient(int client_socket , int client_id) {
    char buffer[BUFFER_SIZE];

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);

        if (bytes_read <= 0) {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "Client " << client_id << " disconnected." << std::endl;
            close(client_socket);
            break;
        }

        std::string message(buffer);
        Order order = split_input(message , client_id);

        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "Message from client " << client_id << " : " << message << std::endl;




        std::string response = "Order Confirmed : [" +
            std::string(order.type) + " | " +
            std::string(order.side) + " | " +
            std::to_string(order.qty) + " | " +
            std::to_string(order.price) + "]";
        send(client_socket, response.c_str(), response.length(), 0);
        orderBook.displayOrderBook();
    }
}
**/

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addr_len = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) != 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "Server is running on port " << PORT << " and waiting for connections..." << std::endl;
    }

    std::thread matchingThread(&OrderBook::matchOrders, &orderBook);
    matchingThread.detach();

    while (true) {
        if ((client_socket = accept(server_fd, (struct sockaddr*)&address, &addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }
            
        int client_id = client_id_counter.fetch_add(1); // Assign a unique ID to the client

        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "New client connected. Assigned Client ID: " << client_id << std::endl;
        }

        std::thread(handleClient, client_socket, client_id).detach();
        //client_threads.emplace_back(std::thread(handleClient, client_socket, client_id)).detach();
    }

    close(server_fd);
    return 0;
}








/**
#include "OrderBook.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <mutex>
#include <csignal>

#define PORT 8080
#define BUFFER_SIZE 1024

OrderBook orderBook;
std::mutex orderBookMutex;
volatile bool keepRunning = true;

// Define the Order struct
struct Order {
    char type[10];   // "M" or "L"
    char side[10];   // "B" or "S"
    int quantity;    // Quantity of the order
    double price;    // Price of the order
};

// Signal handler
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received. Shutting down..." << std::endl;
    keepRunning = false;
}

// Function to handle client communication
void handleClient(int clientSocket) {
    while (true) {
        Order order;
        memset(&order, 0, sizeof(order));

        int bytesRead = recv(clientSocket, &order, sizeof(order), 0);
        if (bytesRead <= 0) {
            std::cout << "Client disconnected." << std::endl;
            close(clientSocket);
            break;
        }

        std::cout << "Received order: [" << order.type << " | " << order.side
            << " | " << order.quantity << " | " << order.price << "]" << std::endl;

        // Protect order book operations with a mutex
        {
            std::lock_guard<std::mutex> lock(orderBookMutex);
            if (strcmp(order.type, "Limit") == 0) {
                orderBook.addOrder(order.side, order.price, order.quantity);
            }
            else if (strcmp(order.type, "Market") == 0) {
                std::cout << "Market orders to be implemented next." << std::endl;
            }
            else {
                std::cerr << "Invalid order type received." << std::endl;
            }
        }

        // Send acknowledgment to client
        std::string response = "Order processed successfully.";
        send(clientSocket, response.c_str(), response.size(), 0);
    }
}

int main() {
    signal(SIGINT, signalHandler);

    int serverFd, newSocket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket
    if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(serverFd, SOMAXCONN) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server is running and waiting for connections on port " << PORT << std::endl;

    // Start the order matching thread
    std::thread matchThread(&OrderBook::matchOrders, &orderBook);
    matchThread.detach();

    while (keepRunning) {
        newSocket = accept(serverFd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (newSocket < 0) {
            if (!keepRunning) break;
            perror("Accept failed");
            continue;
        }

        std::cout << "New client connected." << std::endl;
        std::thread clientThread(handleClient, newSocket);
        clientThread.detach();
    }

    close(serverFd);
    std::cout << "Server shut down gracefully." << std::endl;
    return 0;
}

**/
