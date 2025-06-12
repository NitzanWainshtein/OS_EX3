/**
 * Q4 - Multi-Client Convex Hull Server
 * Single-threaded server handling multiple clients with shared graph
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <string>
#include <sstream>
#include <map>
#include <queue>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace std;

#define PORT 9034
#define BACKLOG 10
#define MAX_BUFFER_SIZE 1024

// 2D Point structure
struct Point {
    double x, y;
    Point() : x(0), y(0) {}
    Point(double x, double y) : x(x), y(y) {}
};

// Queued command for clients waiting for graph access
struct PendingCommand {
    int clientSocket;
    string commandText;
    PendingCommand(int socket, const string& command) : clientSocket(socket), commandText(command) {}
};

// Global shared state
vector<Point> sharedGraphPoints;
bool isGraphLocked = false;
int lockingClientSocket = -1;

// Client state management for multi-step commands
map<int, int> clientInputState;        // 0=normal, 1=reading_points_for_newgraph
map<int, int> pointsToRead;           // How many points client needs to input
map<int, int> pointsAlreadyRead;      // How many points client has already input

// Command queue for operations waiting for graph access
queue<PendingCommand> waitingCommands;

// Convex Hull Algorithm Implementation
double crossProduct(const Point& origin, const Point& pointA, const Point& pointB) {
    return (pointA.x - origin.x) * (pointB.y - origin.y) - (pointA.y - origin.y) * (pointB.x - origin.x);
}

bool comparePointsLexicographically(const Point& a, const Point& b) {
    return (a.x != b.x) ? a.x < b.x : a.y < b.y;
}

vector<Point> computeConvexHull(vector<Point> inputPoints) {
    int numPoints = inputPoints.size();
    if (numPoints <= 1) return inputPoints;
    
    sort(inputPoints.begin(), inputPoints.end(), comparePointsLexicographically);
    
    // Build lower hull
    vector<Point> hullPoints;
    for (int i = 0; i < numPoints; i++) {
        while (hullPoints.size() >= 2 && 
               crossProduct(hullPoints[hullPoints.size()-2], hullPoints[hullPoints.size()-1], inputPoints[i]) <= 0) {
            hullPoints.pop_back();
        }
        hullPoints.push_back(inputPoints[i]);
    }
    
    // Build upper hull
    size_t lowerHullSize = hullPoints.size();
    for (int i = numPoints - 2; i >= 0; i--) {
        while (hullPoints.size() >= lowerHullSize + 1 && 
               crossProduct(hullPoints[hullPoints.size()-2], hullPoints[hullPoints.size()-1], inputPoints[i]) <= 0) {
            hullPoints.pop_back();
        }
        hullPoints.push_back(inputPoints[i]);
    }
    
    if (hullPoints.size() > 1) hullPoints.pop_back();
    return hullPoints;
}

double calculatePolygonArea(const vector<Point>& polygonVertices) {
    if (polygonVertices.size() < 3) return 0.0;
    
    double area = 0.0;
    int numVertices = polygonVertices.size();
    
    for (int i = 0; i < numVertices; i++) {
        int nextIndex = (i + 1) % numVertices;
        area += polygonVertices[i].x * polygonVertices[nextIndex].y;
        area -= polygonVertices[nextIndex].x * polygonVertices[i].y;
    }
    
    return abs(area) / 2.0;
}

// Parse point from "x,y" format
Point parsePointFromString(const string& pointString) {
    size_t commaPosition = pointString.find(',');
    double x = stod(pointString.substr(0, commaPosition));
    double y = stod(pointString.substr(commaPosition + 1));
    return Point(x, y);
}

// Send message to specific client
void sendMessageToClient(int clientSocket, const string& message) {
    string formattedMessage = message + "\n";
    send(clientSocket, formattedMessage.c_str(), formattedMessage.length(), 0);
    cout << "Sent to client " << clientSocket << ": " << message << endl;
}

// Forward declaration
void executeClientCommand(int clientSocket, const string& command);

// Process all queued commands when graph becomes available
void processWaitingCommands() {
    if (isGraphLocked || waitingCommands.empty()) return;
    
    cout << "Processing " << waitingCommands.size() << " waiting commands..." << endl;
    
    while (!waitingCommands.empty() && !isGraphLocked) {
        PendingCommand nextCommand = waitingCommands.front();
        waitingCommands.pop();
        
        cout << "Executing waiting command from client " << nextCommand.clientSocket 
             << ": " << nextCommand.commandText << endl;
        
        executeClientCommand(nextCommand.clientSocket, nextCommand.commandText);
        
        // Stop if graph got locked again
        if (isGraphLocked) {
            cout << "Command processing paused - graph locked again" << endl;
            break;
        }
    }
    
    if (waitingCommands.empty()) {
        cout << "All waiting commands processed." << endl;
    }
}

// Execute command immediately (assumes graph is available)
void executeClientCommand(int clientSocket, const string& command) {
    
    // Handle "Newgraph n" command
    if (command.substr(0, 9) == "Newgraph ") {
        isGraphLocked = true;
        lockingClientSocket = clientSocket;
        cout << "Graph locked by client " << clientSocket << endl;
        
        int numberOfPoints = stoi(command.substr(9));
        sharedGraphPoints.clear();
        sendMessageToClient(clientSocket, "Enter " + to_string(numberOfPoints) + " points (x,y):");
        
        clientInputState[clientSocket] = 1;
        pointsToRead[clientSocket] = numberOfPoints;
        pointsAlreadyRead[clientSocket] = 0;
    }
    
    // Handle "CH" command
    else if (command == "CH") {
        if (sharedGraphPoints.size() < 3) {
            sendMessageToClient(clientSocket, "0");
        } else {
            cout << "Client " << clientSocket << " computing convex hull..." << endl;
            vector<Point> hullVertices = computeConvexHull(sharedGraphPoints);
            double hullArea = calculatePolygonArea(hullVertices);
            
            ostringstream areaStream;
            areaStream << fixed << setprecision(1) << hullArea;
            sendMessageToClient(clientSocket, areaStream.str());
        }
    }
    
    // Handle "Newpoint x,y" command
    else if (command.substr(0, 9) == "Newpoint ") {
        isGraphLocked = true;
        lockingClientSocket = clientSocket;
        cout << "Graph locked by client " << clientSocket << endl;
        
        Point newPoint = parsePointFromString(command.substr(9));
        sharedGraphPoints.push_back(newPoint);
        sendMessageToClient(clientSocket, "Point added");
        cout << "Point (" << newPoint.x << "," << newPoint.y << ") added" << endl;
        
        isGraphLocked = false;
        lockingClientSocket = -1;
        cout << "Graph unlocked" << endl;
        processWaitingCommands();
    }
    
    // Handle "Removepoint x,y" command
    else if (command.substr(0, 12) == "Removepoint ") {
        isGraphLocked = true;
        lockingClientSocket = clientSocket;
        cout << "Graph locked by client " << clientSocket << endl;
        
        Point targetPoint = parsePointFromString(command.substr(12));
        
        // Find and remove the point
        for (int i = sharedGraphPoints.size() - 1; i >= 0; i--) {
            if (abs(sharedGraphPoints[i].x - targetPoint.x) < 1e-9 && 
                abs(sharedGraphPoints[i].y - targetPoint.y) < 1e-9) {
                sharedGraphPoints.erase(sharedGraphPoints.begin() + i);
                break;
            }
        }
        
        sendMessageToClient(clientSocket, "Point removed");
        cout << "Point (" << targetPoint.x << "," << targetPoint.y << ") removed" << endl;
        
        isGraphLocked = false;
        lockingClientSocket = -1;
        cout << "Graph unlocked" << endl;
        processWaitingCommands();
    }
}

// Process command from client
void handleClientCommand(int clientSocket, const string& rawCommand) {
    string cleanCommand = rawCommand;
    
    // Remove trailing whitespace
    while (!cleanCommand.empty() && (cleanCommand.back() == ' ' || cleanCommand.back() == '\t' || cleanCommand.back() == '\r')) {
        cleanCommand.pop_back();
    }
    
    if (cleanCommand.empty()) return;
    
    cout << "Client " << clientSocket << " command: " << cleanCommand << endl;
    
    // Handle point input during Newgraph command
    if (clientInputState[clientSocket] == 1) {
        Point inputPoint = parsePointFromString(cleanCommand);
        sharedGraphPoints.push_back(inputPoint);
        pointsAlreadyRead[clientSocket]++;
        
        sendMessageToClient(clientSocket, "Point " + to_string(pointsAlreadyRead[clientSocket]) + " accepted");
        
        if (pointsAlreadyRead[clientSocket] >= pointsToRead[clientSocket]) {
            sendMessageToClient(clientSocket, "Graph created with " + to_string(pointsAlreadyRead[clientSocket]) + " points");
            cout << "Shared graph updated: " << sharedGraphPoints.size() << " points" << endl;
            
            clientInputState[clientSocket] = 0;
            isGraphLocked = false;
            lockingClientSocket = -1;
            cout << "Graph unlocked" << endl;
            processWaitingCommands();
        }
        return;
    }
    
    // Check if command requires graph access
    bool requiresGraphAccess = (cleanCommand.substr(0, 9) == "Newgraph ") ||
                              (cleanCommand.substr(0, 9) == "Newpoint ") ||
                              (cleanCommand.substr(0, 12) == "Removepoint ") ||
                              (cleanCommand == "CH");
    
    // Queue command if graph is busy
    if (isGraphLocked && requiresGraphAccess) {
        waitingCommands.push(PendingCommand(clientSocket, cleanCommand));
        cout << "Queuing command from client " << clientSocket << ": " << cleanCommand << endl;
        sendMessageToClient(clientSocket, "Command queued (position " + to_string(waitingCommands.size()) + ")");
        return;
    }
    
    // Execute command immediately
    cout << "Executing immediate command from client " << clientSocket << ": " << cleanCommand << endl;
    executeClientCommand(clientSocket, cleanCommand);
}

int main() {
    int serverSocket;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t clientAddressSize;
    int socketOption = 1;
    
    fd_set masterSocketSet, readSocketSet;
    int maxSocketDescriptor;
    map<int, string> clientInputBuffers;
    
    cout << "=== Multi-Client Convex Hull Server ===" << endl;
    cout << "Port: " << PORT << endl;
    
    // Create and configure server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &socketOption, sizeof(int));
    
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    memset(serverAddress.sin_zero, '\0', sizeof serverAddress.sin_zero);
    
    bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof serverAddress);
    listen(serverSocket, BACKLOG);
    
    cout << "Server ready! Shared graph: " << sharedGraphPoints.size() << " points" << endl;
    cout << "Waiting for clients..." << endl;
    
    // Initialize socket sets for select()
    FD_ZERO(&masterSocketSet);
    FD_SET(serverSocket, &masterSocketSet);
    maxSocketDescriptor = serverSocket;
    
    // Main server loop using select()
    while (true) {
        readSocketSet = masterSocketSet;
        select(maxSocketDescriptor + 1, &readSocketSet, NULL, NULL, NULL);
        
        for (int currentSocket = 0; currentSocket <= maxSocketDescriptor; currentSocket++) {
            if (FD_ISSET(currentSocket, &readSocketSet)) {
                
                // Handle new client connection
                if (currentSocket == serverSocket) {
                    clientAddressSize = sizeof clientAddress;
                    int newClientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientAddressSize);
                    
                    FD_SET(newClientSocket, &masterSocketSet);
                    if (newClientSocket > maxSocketDescriptor) maxSocketDescriptor = newClientSocket;
                    
                    cout << "New client " << newClientSocket << " connected" << endl;
                    sendMessageToClient(newClientSocket, "Convex Hull Server");
                    sendMessageToClient(newClientSocket, "Commands: Newgraph n, CH, Newpoint x,y, Removepoint x,y");
                    clientInputBuffers[newClientSocket] = "";
                }
                
                // Handle data from existing client
                else {
                    char dataBuffer[MAX_BUFFER_SIZE];
                    int bytesReceived = recv(currentSocket, dataBuffer, sizeof(dataBuffer), 0);
                    
                    if (bytesReceived <= 0) {
                        // Client disconnected
                        cout << "Client " << currentSocket << " disconnected" << endl;
                        
                        // Release graph lock if held by this client
                        if (lockingClientSocket == currentSocket) {
                            isGraphLocked = false;
                            lockingClientSocket = -1;
                            cout << "Graph unlocked (client disconnected)" << endl;
                            processWaitingCommands();
                        }
                        
                        // Remove client's commands from queue
                        queue<PendingCommand> filteredQueue;
                        while (!waitingCommands.empty()) {
                            PendingCommand command = waitingCommands.front();
                            waitingCommands.pop();
                            if (command.clientSocket != currentSocket) {
                                filteredQueue.push(command);
                            }
                        }
                        waitingCommands = filteredQueue;
                        
                        // Clean up client data
                        close(currentSocket);
                        FD_CLR(currentSocket, &masterSocketSet);
                        clientInputBuffers.erase(currentSocket);
                        clientInputState.erase(currentSocket);
                        pointsToRead.erase(currentSocket);
                        pointsAlreadyRead.erase(currentSocket);
                    } else {
                        // Process received data
                        dataBuffer[bytesReceived] = '\0';
                        clientInputBuffers[currentSocket] += string(dataBuffer);
                        
                        // Process complete lines
                        string& clientBuffer = clientInputBuffers[currentSocket];
                        size_t newlinePosition;
                        
                        while ((newlinePosition = clientBuffer.find('\n')) != string::npos) {
                            string commandLine = clientBuffer.substr(0, newlinePosition);
                            clientBuffer.erase(0, newlinePosition + 1);
                            
                            handleClientCommand(currentSocket, commandLine);
                        }
                    }
                }
            }
        }
    }
    
    close(serverSocket);
    return 0;
}