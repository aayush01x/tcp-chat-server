# Chat Server with Groups and Private Messages

## Overview

This project is a simple chat server written in C++. It allows multiple users to connect over TCP, log in using a username and password, and chat with each other. 

The server supports private messages, broadcasts to everyone, and group chats. It also notifies users when someone joins or leaves.

---

## How to Build and Run

### Building the Server and Client

This project uses a Makefile to compile both the server and the client. M To compile the server and client, simply run:

```bash
make
```
This will build the executables server_grp and client_grp.

### Running the Server

Once the server is compiled, start it by running:

```bash
./server_grp
```

The server will listen for connections on port **12345**.

### Connecting as a Client

Once the client is compiled, start it by running:

```bash
./client_grp
```
The client will prompt you for a username and password to log into the chat server. After authentication, you can use the available commands to send messages and join groups.

---

## Features

- **User Login and Authentication**  
  When you connect, you'll be asked for a username and password. Only one login per user is allowed at a time.

- **Real-Time Notifications**  
  When a user logs in or disconnects, everyone else gets a notification.

- **Private Messaging**  
  Send a private message to a specific user using:
  ```
  /msg <username> <message>
  ```

- **Broadcast Messaging**  
  Send a message to all users with:
  ```
  /broadcast <message>
  ```

- **Group Chat**  
  - Create a group:  
    ```
    /create_group <group_name>
    ```
  - Join a group:  
    ```
    /join_group <group_name>
    ```
  - Send a group message:  
    ```
    /group_msg <group_name> <message>
    ```
  - Leave a group:  
    ```
    /leave_group <group_name>
    ```

- **Clean Disconnects**  
  When a user disconnects, the server removes them from the list and any groups they joined, and notifies everyone else.

---

## Technical Details

### Concurrency Model
- The server follows a **thread-per-client** approach using `std::thread`. Each new client connection spawns a dedicated thread for handling communication.
- Shared resources such as active user lists and group memberships are protected using `std::mutex` to prevent race conditions.

### Data Structures
- **User Authentication:**
  - A `std::unordered_map<std::string, std::string>` stores username-password pairs.
- **Active Clients:**
  - A `std::unordered_map<int, std::string>` maps socket descriptors to usernames.
  - A `std::unordered_map<std::string, int>` maps usernames to socket descriptors.
- **Group Management:**
  - A `std::unordered_map<std::string, std::unordered_set<int>>` maintains active groups and their members.

### Network Communication
- The server employs **blocking socket calls** (`recv()` and `send()`) for message exchange.
- Each client communicates using a persistent TCP connection with the server.
- SIGINT (`Ctrl+C`) triggers a signal handler that ensures proper cleanup before termination.

---

## Testing

### What all was tested?

- **Login Process:**  
  Made sure valid credentials were allowing logging in and invalid where showing Authentication failed.

- **Messaging:**  
  Checked that private messages go only to the intended person, and broadcasts reach everyone.

- **Group Features:**  
Creation, joining and leaving of group was working as expected. Also group messages was also working as expected.


## Stress Testing

To evaluate the performance and reliability of the server under heavy load, I conducted extensive stress testing using automated scripts and multiple concurrent client connections.

### Methodology
- **Simultaneous Clients:**  
  - Launched **500+ clients** using a shell script to automate multiple client connections.
  - Each client logged in with a unique username and began sending messages.
  
- **High-Frequency Messaging:**  
  - Each client sent **continuous private messages** and **broadcasts** to simulate a high-volume chat environment.
  - Group chats were stress-tested by having multiple users join and send messages in rapid succession.

- **Disconnection & Reconnection Handling:**  
  - Simulated unexpected client crashes and network failures by forcibly terminating client processes.
  - Verified that the server properly removed disconnected clients and allowed them to reconnect.

### Observations & Results
- The server successfully handled **up to 500 concurrent clients** before performance degradation was noticed.
- No race conditions or deadlocks were observed due to proper **mutex protection**.
- The server’s **thread-per-client** model worked efficiently but led to **high memory consumption** as the number of clients increased.

## Declaration 

I hereby declare that this project, including all source code, documentation, and testing methodologies, has been developed by me.

Any references used for learning purposes, such as official documentation, networking guides, or C++ reference materials, have been acknowledged.

## References
- **C++ Concurrency in Action by Anthony Williams**: This book helped me understand how to implement multi-threading in C++ and how to manage concurrent client connections effectively.

- **Beej’s Guide to Network Programming** and  **UNIX Network Programming: Volume 1: The Sockets Networking API** by W. Richard Stevens: These provided me with the foundational knowledge of sockets and network programming in C++.

- Stackoverflow
- Youtube tutorials
- GeeksforGeeks
