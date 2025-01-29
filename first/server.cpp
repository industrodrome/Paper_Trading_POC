#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

std::mutex console_mutex;

// Function to handle client communication
void handleClient(int client_socket) {
    char buffer[BUFFER_SIZE];

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);

        if (bytes_read <= 0) {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "Client disconnected." << std::endl;
            close(client_socket);
            break;
        }

        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "Message from client: " << buffer << std::endl;
        }

        // Echo the message back to the client
        std::string response = "Server received: " + std::string(buffer);
        send(client_socket, response.c_str(), response.length(), 0);
    }
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addr_len = sizeof(address);

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) != 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "Server is running on port " << PORT << " and waiting for connections..." << std::endl;
    }

    // Vector to hold client threads
    std::vector<std::thread> client_threads;

    // Accept incoming client connections
    while (true) {
        if ((client_socket = accept(server_fd, (struct sockaddr*)&address, &addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "New client connected." << std::endl;
        }

        // Create a thread to handle the new client
        client_threads.emplace_back(std::thread(handleClient, client_socket));
    }

    // Join all client threads before exiting (optional in this example, as it's infinite loop)
    for (auto& t : client_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    close(server_fd);
    return 0;
}
