#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

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

    std::cout << "Connected to the server. Type your message below." << std::endl;

    while (true) {
        // Get user input
        std::string user_input;
        std::cout << "Enter message: ";
        std::getline(std::cin, user_input);

        // Send message to the server
        send(sock, user_input.c_str(), user_input.length(), 0);

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
