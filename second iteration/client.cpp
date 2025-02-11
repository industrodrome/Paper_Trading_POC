//client

#include"OrderBook.h"
#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

OrderBook orderBook;

int main() {
    int sock = 0;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = { 0 };

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error." << std::endl;
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported." << std::endl;
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Connection to the server failed." << std::endl;
        return -1;
    }

    std::cout << "Connected to the server." << std::endl;

    while (true) {
        // Get user input
        std::string user_input;
        std::cout << "Place Order: (Limit or Market) | (Buy or Sell) | (Quantity) | (Price > 0 if Limit else 0) \n\n";
        std::getline(std::cin, user_input);

        // Send message to the server
        send(sock, user_input.c_str(), user_input.length(), 0);

        //orderBook.displayOrderBook();

        // Exit the loop if the user types "exit"
        if (user_input == "exit") {
            std::cout << "Disconnecting from the server..." << std::endl;
            break;
        }

        // Receive server response
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = read(sock, buffer, BUFFER_SIZE - 1);
        if (bytes_received > 0) {
            std::cout << "Server: " << buffer << std::endl;
        }
        else {
            std::cerr << "Server disconnected or error occurred." << std::endl;
            break;
        }
    }

    // Close the socket
    close(sock);
    return 0;
}





/**
#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits>

#define PORT 8080
#define BUFFER_SIZE 1024

struct Order {
    char type[10];   // e.g., "M", "L"
    char side[10];   // e.g., "B", "S"
    int quantity;    // Quantity of the order
    double price;    // Price of the order
};

int main() {
    int sock = 0;
    struct sockaddr_in servAddr;
    char buffer[BUFFER_SIZE] = { 0 };

    // Create the socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    // Set up the server address structure
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &servAddr.sin_addr) <= 0) {
        perror("Invalid address or Address not supported");
        return -1;
    }

    // Attempt to connect to the server
    std::cout << "Attempting to connect to the server at 127.0.0.1:8080..." << std::endl;
    if (connect(sock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    std::cout << "Connected to the server.\nEnter orders with individual fields (Type, Side, Qty, Price).\nType 'exit' to disconnect.\n\n";

    while (true) {
        Order order;
        memset(&order, 0, sizeof(order));

        // Get order type
        std::cout << "Enter order type (e.g., Market, Limit): ";
        std::cin >> order.type;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        // Check for exit condition
        if (strcmp(order.type, "exit") == 0) {
            std::cout << "Disconnecting from the server..." << std::endl;
            break;
        }

        // Get order details
        std::cout << "Enter side (Buy/Sell): ";
        std::cin >> order.side;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        std::cout << "Enter quantity: ";
        std::cin >> order.quantity;
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid quantity. Please try again." << std::endl;
            continue;
        }

        std::cout << "Enter price: ";
        std::cin >> order.price;
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid price. Please try again." << std::endl;
            continue;
        }

        // Send the order to the server
        if (send(sock, &order, sizeof(order), 0) < 0) {
            perror("Failed to send the order");
            break;
        }
        std::cout << "Order sent: [" << order.type << " | " << order.side << " | "
            << order.quantity << " | " << order.price << "]" << std::endl;

        // Wait for the server's response
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = read(sock, buffer, BUFFER_SIZE - 1);
        if (bytesReceived < 0) {
            perror("Failed to receive response from the server");
            break;
        }
        else if (bytesReceived == 0) {
            std::cerr << "Server closed the connection." << std::endl;
            break;
        }

        std::cout << "Server response: " << buffer << std::endl;
    }

    // Close the socket before exiting
    close(sock);
    return 0;
}
**/