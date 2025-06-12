/**
 * Multi-Client Convex Hull Client
 * Connects to convex hull server and provides interactive interface
 */

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>

using namespace std;

#define PORT 9034
#define SERVER_IP "127.0.0.1"

// Send command to server
void sendCommandToServer(int serverSocket, const string& command) {
    string formattedCommand = command + "\n";
    send(serverSocket, formattedCommand.c_str(), formattedCommand.length(), 0);
}

// Receive response from server with timeout
string receiveServerResponse(int serverSocket, int timeoutMs = 1000) {
    string response = "";
    char character;
    
    // Set socket to non-blocking mode
    int socketFlags = fcntl(serverSocket, F_GETFL, 0);
    fcntl(serverSocket, F_SETFL, socketFlags | O_NONBLOCK);
    
    fd_set readFileDescriptors;
    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    
    while (true) {
        FD_ZERO(&readFileDescriptors);
        FD_SET(serverSocket, &readFileDescriptors);
        
        int selectResult = select(serverSocket + 1, &readFileDescriptors, NULL, NULL, &timeout);
        
        if (selectResult > 0 && FD_ISSET(serverSocket, &readFileDescriptors)) {
            if (recv(serverSocket, &character, 1, 0) > 0) {
                if (character == '\n') {
                    break;
                }
                if (character != '\r') {
                    response += character;
                }
            } else {
                break;
            }
        } else {
            // Timeout or error occurred
            break;
        }
    }
    
    // Restore blocking mode
    fcntl(serverSocket, F_SETFL, socketFlags);
    
    return response;
}

// Check if server has more messages waiting
bool hasMoreServerMessages(int serverSocket) {
    fd_set readFileDescriptors;
    struct timeval shortTimeout;
    shortTimeout.tv_sec = 0;
    shortTimeout.tv_usec = 100000; // 100ms
    
    FD_ZERO(&readFileDescriptors);
    FD_SET(serverSocket, &readFileDescriptors);
    
    return select(serverSocket + 1, &readFileDescriptors, NULL, NULL, &shortTimeout) > 0;
}

int main() {
    int clientSocket;
    struct sockaddr_in serverAddress;
    
    cout << "=== Convex Hull Client ===" << endl;
    cout << "Connecting to server at " << SERVER_IP << ":" << PORT << "..." << endl;
    
    // Create client socket
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Configure server address
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = inet_addr(SERVER_IP);
    memset(serverAddress.sin_zero, '\0', sizeof serverAddress.sin_zero);
    
    // Connect to server
    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof serverAddress) == -1) {
        perror("Connection failed");
        exit(1);
    }
    
    cout << "Connected to server successfully!" << endl;
    cout << "========================================" << endl;
    
    // Receive welcome messages from server
    string welcomeMessage1 = receiveServerResponse(clientSocket);
    string welcomeMessage2 = receiveServerResponse(clientSocket);
    cout << "Server: " << welcomeMessage1 << endl;
    cout << "Server: " << welcomeMessage2 << endl;
    
    cout << "========================================" << endl;
    cout << "Available commands:" << endl;
    cout << "  Newgraph n       - Create new graph with n points" << endl;
    cout << "  CH               - Calculate convex hull area" << endl;
    cout << "  Newpoint x,y     - Add point to graph" << endl;
    cout << "  Removepoint x,y  - Remove point from graph" << endl;
    cout << "  quit/exit        - Disconnect from server" << endl;
    cout << "========================================" << endl;
    
    string userCommand;
    while (true) {
        cout << ">> ";
        getline(cin, userCommand);
        
        if (userCommand == "quit" || userCommand == "exit") {
            cout << "Disconnecting from server..." << endl;
            break;
        }
        
        if (userCommand.empty()) continue;
        
        // Send command to server
        sendCommandToServer(clientSocket, userCommand);
        
        // Receive all responses from server
        do {
            string serverResponse = receiveServerResponse(clientSocket, 2000); // 2 second timeout
            if (!serverResponse.empty()) {
                cout << "Server: " << serverResponse << endl;
            }
        } while (hasMoreServerMessages(clientSocket));
    }
    
    close(clientSocket);
    cout << "Goodbye!" << endl;
    return 0;
}