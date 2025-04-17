#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <csignal>

std::unordered_map<int, std::string> socket_to_username;
std::unordered_map<std::string, int> username_to_socket;
std::unordered_map<std::string, std::string> users; 
std::unordered_map<std::string, std::unordered_set<int>> groups; 
std::mutex client_mutex, group_mutex;

bool server_running = true;
int server_socket = -1;


void send_message(int client_socket, const std::string& message) {
    if (send(client_socket, message.c_str(), message.size(), 0) <= 0) {
        std::cerr << "Error sending message to client " << client_socket << std::endl;
    }
}

void user_loader(const std::string& filename) {
    std::ifstream file(filename);
    
    if (!file) {
        std::cerr << "Error: Unable to open users.txt" << std::endl;
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream input_stream(line);
        std::string username, password;
        if (std::getline(input_stream, username, ':') && std::getline(input_stream, password)) {
            users[username] = password;
        }
    }
}

void client_handler(int client_socket) {
    char recv_buffer[1024];
    std::string username;

    // Authentication
    send_message(client_socket, "Enter username: ");
    memset(recv_buffer, 0, 1024);
    if (recv(client_socket, recv_buffer, 1024, 0) <= 0) {
        close(client_socket);
        return;
    }
    username = recv_buffer;

    send_message(client_socket, "Enter password: ");
    memset(recv_buffer, 0, 1024);
    if (recv(client_socket, recv_buffer, 1024, 0) <= 0) {
        close(client_socket);
        return;
    }
    std::string password = recv_buffer;

    // Validate credentials
    if (users.find(username) == users.end() || users[username] != password) {
        send_message(client_socket, "Authentication failed.");
        close(client_socket);
        return;
    }

    // Check for existing login
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        if (username_to_socket.count(username)) {
            send_message(client_socket, "User already logged in.");
            close(client_socket);
            return;
        }
        socket_to_username[client_socket] = username;
        username_to_socket[username] = client_socket;
    }

    send_message(client_socket, "Welcome to the chat server!\n");

    // Notify active users
    std::vector<int> active_sockets;
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        active_sockets.reserve(socket_to_username.size());
        for (const auto& [sock, _] : socket_to_username) {
            if (sock != client_socket) active_sockets.push_back(sock);
        }
    }

    if (!active_sockets.empty()) {
        std::string active_users = "Active users: ";
        for (int sock : active_sockets) active_users += socket_to_username[sock] + ", ";
        active_users = active_users.substr(0, active_users.size()-2);
        send_message(client_socket, active_users);
    } else {
        send_message(client_socket, "No other users are currently active.");
    }

    // Notify others of new user
    std::string join_message = username + " has joined the chat.";
    for (int sock : active_sockets) {
        send_message(sock, join_message);
    }

    // Handle client commands
    while (true) {
        memset(recv_buffer, 0, 1024);
        int bytes_received = recv(client_socket, recv_buffer, 1024, 0);
        if (bytes_received <= 0) break;

        std::string message(recv_buffer);
        if (message.rfind("/msg ", 0) == 0) {
            size_t space_pos = message.find(' ', 5);
            if (space_pos != std::string::npos) {
                std::string target_user = message.substr(5, space_pos - 5);
                std::string private_msg = message.substr(space_pos + 1);
                
                std::lock_guard<std::mutex> lock(client_mutex);
                auto it = username_to_socket.find(target_user);
                if (it != username_to_socket.end()) {
                    send_message(it->second, "[Private] " + username + ": " + private_msg);
                } else {
                    send_message(client_socket, "User not found.");
                }
            }
        }
        else if (message.rfind("/broadcast ", 0) == 0) {
            std::string broadcast_msg = username + ": " + message.substr(11);
            std::vector<int> recipients;
            {
                std::lock_guard<std::mutex> lock(client_mutex);
                recipients.reserve(socket_to_username.size());
                for (const auto& [sock, _] : socket_to_username) {
                    recipients.push_back(sock);
                }
            }
            for (int sock : recipients) {
                send_message(sock, broadcast_msg);
            }
        }
        else if (message.rfind("/create_group ", 0) == 0) {
            std::string group_name = message.substr(14);
            if (group_name.empty()) {
                send_message(client_socket, "Invalid group name.");
                continue;
            }

            std::lock_guard<std::mutex> lock(group_mutex);
            if (groups.find(group_name) == groups.end()) {
                groups[group_name].insert(client_socket);
                send_message(client_socket, "Group " + group_name + " created.");
                
                // Notify other clients
                std::vector<int> recipients;
                {
                    std::lock_guard<std::mutex> client_lock(client_mutex);
                    for (const auto& [sock, _] : socket_to_username) {
                        if (sock != client_socket) recipients.push_back(sock);
                    }
                }
                std::string notice = username + " created group " + group_name;
                for (int sock : recipients) {
                    send_message(sock, notice);
                }
            } else {
                send_message(client_socket, "Group already exists.");
            }
        }
        else if (message.rfind("/join_group ", 0) == 0) {
            std::string group_name = message.substr(12);
            std::unordered_set<int> members;
            
            {
                std::lock_guard<std::mutex> lock(group_mutex);
                auto it = groups.find(group_name);
                if (it == groups.end()) {
                    send_message(client_socket, "Group not found.");
                    continue;
                }
                it->second.insert(client_socket);
                members = it->second;
            }
            
            send_message(client_socket, "You joined the group " + group_name + ".");
            std::string notice = username + " joined group " + group_name;
            for (int sock : members) {
                if (sock != client_socket) send_message(sock, notice);
            }
        }
        else if (message.rfind("/group_msg ", 0) == 0) {
            size_t space_pos = message.find(' ', 11);
            if (space_pos == std::string::npos) {
                send_message(client_socket, "Invalid command.");
                continue;
            }
            
            std::string group_name = message.substr(11, space_pos - 11);
            std::string msg_content = message.substr(space_pos + 1);
            std::unordered_set<int> members;
            
            {
                std::lock_guard<std::mutex> lock(group_mutex);
                auto it = groups.find(group_name);
                if (it == groups.end() || !it->second.count(client_socket)) {
                    send_message(client_socket, "Not in group or group doesn't exist.");
                    continue;
                }
                members = it->second;
            }
            
            std::string formatted_msg = "[Group " + group_name + "] " + ": " + msg_content;
            for (int sock : members) {
                send_message(sock, formatted_msg);
            }
        }
        else if (message.rfind("/leave_group ", 0) == 0) {
            std::string group_name = message.substr(13);
            std::unordered_set<int> members;
            bool was_in_group = false;
            
            {
                std::lock_guard<std::mutex> lock(group_mutex);
                auto it = groups.find(group_name);
                if (it != groups.end() && it->second.erase(client_socket)) {
                    was_in_group = true;
                    members = it->second;
                    if (it->second.empty()) {
                        groups.erase(it);
                    }
                }
            }
            
            if (was_in_group) {
                send_message(client_socket, "Left group " + group_name);
                std::string notice = username + " left group " + group_name;
                for (int sock : members) {
                    send_message(sock, notice);
                }
            } else {
                send_message(client_socket, "Not in group.");
            }
        }
        else {
            send_message(client_socket, "Invalid command.");
        }
    }

    {
        std::lock_guard<std::mutex> lock(client_mutex);
        if (socket_to_username.erase(client_socket)) {
            username_to_socket.erase(username);
        }
    }

    std::unordered_map<std::string, std::unordered_set<int>> groups_copy;
    {
        std::lock_guard<std::mutex> lock(group_mutex);
        groups_copy = groups;
    }
    
    for (auto& [group_name, members] : groups_copy) {
        bool was_in_group = false;
        {
            std::lock_guard<std::mutex> lock(group_mutex);
            auto it = groups.find(group_name);
            if (it != groups.end() && it->second.erase(client_socket)) {
                was_in_group = true;
                if (it->second.empty()) {
                    groups.erase(it);
                }
            }
        }
        
        if (was_in_group) {
            std::string notice = username + " has left group " + group_name + " (disconnected).";
            for (int sock : members) {
                if (sock != client_socket) send_message(sock, notice);
            }
        }
    }

    std::string leave_msg = username + " has left the chat.";
    std::vector<int> active_clients;
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        active_clients.reserve(socket_to_username.size());
        for (const auto& [sock, _] : socket_to_username) {
            active_clients.push_back(sock);
        }
    }
    for (int sock : active_clients) {
        send_message(sock, leave_msg);
    }

    close(client_socket);
}

void signal_handler(int signal) {
    (void)signal;
    server_running = false;
    std::cout << "Sever is shutting down..." << std::endl;
    shutdown(server_socket, SHUT_RDWR);
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        for (auto& [sock, _] : socket_to_username) {
            shutdown(sock, SHUT_RDWR);
            close(sock);
        }
        socket_to_username.clear();
        username_to_socket.clear();
    }
    {
        std::lock_guard<std::mutex> lock(group_mutex);
        groups.clear();
    }
    
    exit(0);
}

int main() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Error creating server socket." << std::endl;
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(12345);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Error binding server socket." << std::endl;
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 10) == -1) {
        std::cerr << "Error listening on server socket." << std::endl;
        close(server_socket);
        return -1;
    }

    signal(SIGINT, signal_handler);

    user_loader("users.txt");

    std::cout << "Server started. Waiting for clients..." << std::endl;

    while (server_running) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == -1) {
            if (!server_running) break;
            std::cerr << "Error accepting client connection." << std::endl;
            continue;
        }

        std::thread(client_handler, client_socket).detach();
    }

    close(server_socket);
    return 0;
}
