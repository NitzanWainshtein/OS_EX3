/**
 * Step 6 - Convex Hull Server with Reactor (Thread-Safe Fixed Version)
 * --------------------------------------------------------------------
 * This server handles multiple clients using the Reactor pattern from step 5.
 * It implements all command logic from step 4: Newgraph, Newpoint, Removepoint, CH.
 * Uses a shared graph, client input states, and command queuing with proper locking.
 * All race conditions fixed with proper mutex protection.
 */

#include <iostream>
#include <vector>
#include <fcntl.h>
#include <errno.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include <iomanip>
#include <string>
#include <sstream>
#include <map>
#include <queue>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "../q5/Reactor.hpp"

using namespace std;

#define PORT 9034
#define MAX_BUFFER_SIZE 1024

// Basic point structure
struct Point {
    double x, y;
    Point() : x(0), y(0) {}
    Point(double x, double y) : x(x), y(y) {}
};

// Represents a command that must wait for access to the shared graph
struct PendingCommand {
    int clientSocket;
    string commandText;
    PendingCommand(int socket, const string& command) : clientSocket(socket), commandText(command) {}
};

// Global state with proper mutex protection
vector<Point> sharedGraphPoints;
bool isGraphLocked = false;
int lockingClientSocket = -1;
mutex globalStateMutex;  // Protects all global state

// Per-client tracking with mutex protection
map<int, int> clientInputState;      // 0: normal, 1: reading points
map<int, int> pointsToRead;
map<int, int> pointsAlreadyRead;
map<int, string> clientBuffers;
mutex clientDataMutex;  // Protects client tracking data

// Command queue with mutex protection
queue<PendingCommand> waitingCommands;
mutex commandQueueMutex;  // Protects the waiting commands queue

Reactor reactor;
int serverSocket;

// Forward declarations
void executeClientCommand(int clientSocket, const string& command);
void processWaitingCommands();

// Utility functions
Point parsePointFromString(const string& pointString) {
    try {
        size_t comma = pointString.find(',');
        if (comma == string::npos) {
            throw invalid_argument("Invalid point format: missing comma");
        }
        
        string xStr = pointString.substr(0, comma);
        string yStr = pointString.substr(comma + 1);
        
        // Remove whitespace
        xStr.erase(remove_if(xStr.begin(), xStr.end(), ::isspace), xStr.end());
        yStr.erase(remove_if(yStr.begin(), yStr.end(), ::isspace), yStr.end());
        
        if (xStr.empty() || yStr.empty()) {
            throw invalid_argument("Invalid point format: empty coordinate");
        }
        
        double x = stod(xStr);
        double y = stod(yStr);
        
        return Point(x, y);
    } catch (const exception& e) {
        throw invalid_argument("Invalid point format: " + string(e.what()));
    }
}

void sendMessageToClient(int clientSocket, const string& msg) {
    string formatted = msg + "\n";
    ssize_t sent = send(clientSocket, formatted.c_str(), formatted.size(), 0);
    if (sent < 0) {
        cerr << "[sendMessageToClient] Error sending to socket " << clientSocket 
             << ": " << strerror(errno) << endl;
        return;
    }
    cout << "[sendMessageToClient] socket=" << clientSocket << ", message=\"" << msg << "\"" << endl;
}

double crossProduct(const Point& o, const Point& a, const Point& b) {
    return (a.x - o.x)*(b.y - o.y) - (a.y - o.y)*(b.x - o.x);
}

vector<Point> computeConvexHull(vector<Point> pts) {
    if (pts.size() <= 1) return pts;
    
    sort(pts.begin(), pts.end(), [](const Point& a, const Point& b) {
        return (a.x != b.x) ? a.x < b.x : a.y < b.y;
    });

    vector<Point> hull;
    // Build lower hull
    for (const Point& p : pts) {
        while (hull.size() >= 2 && crossProduct(hull[hull.size()-2], hull[hull.size()-1], p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
    
    // Build upper hull
    size_t lower = hull.size();
    for (int i = pts.size()-2; i >= 0; i--) {
        while (hull.size() > lower && crossProduct(hull[hull.size()-2], hull[hull.size()-1], pts[i]) <= 0)
            hull.pop_back();
        hull.push_back(pts[i]);
    }
    if (hull.size() > 1) hull.pop_back();
    return hull;
}

double calculatePolygonArea(const vector<Point>& poly) {
    if (poly.size() < 3) return 0.0;
    
    double area = 0;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        area += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
    }
    return fabs(area) / 2.0;
}

void processWaitingCommands() {
    lock_guard<mutex> queueLock(commandQueueMutex);
    lock_guard<mutex> stateLock(globalStateMutex);
    
    while (!waitingCommands.empty() && !isGraphLocked) {
        auto cmd = waitingCommands.front();
        waitingCommands.pop();
        
        // Execute command without holding locks (recursive call protection)
        globalStateMutex.unlock();
        commandQueueMutex.unlock();
        executeClientCommand(cmd.clientSocket, cmd.commandText);
        commandQueueMutex.lock();
        globalStateMutex.lock();
    }
}

void executeClientCommand(int clientSocket, const string& command) {
    cout << "[executeClientCommand] socket=" << clientSocket << ", command='" << command << "'" << endl;

    try {
        if (command.substr(0, 9) == "Newgraph ") {
            int n;
            try {
                n = stoi(command.substr(9));
                if (n <= 0) {
                    throw invalid_argument("Number of points must be positive");
                }
            } catch (const exception& e) {
                sendMessageToClient(clientSocket, "Error: Invalid number of points");
                return;
            }
            
            {
                lock_guard<mutex> stateLock(globalStateMutex);
                lock_guard<mutex> clientLock(clientDataMutex);
                
                isGraphLocked = true;
                lockingClientSocket = clientSocket;
                sharedGraphPoints.clear();
                
                clientInputState[clientSocket] = 1;
                pointsToRead[clientSocket] = n;
                pointsAlreadyRead[clientSocket] = 0;
            }
            
            sendMessageToClient(clientSocket, "Enter " + to_string(n) + " points (x,y):");
            
        } else if (command == "CH") {
            vector<Point> pointsCopy;
            {
                lock_guard<mutex> stateLock(globalStateMutex);
                pointsCopy = sharedGraphPoints;
            }
            
            if (pointsCopy.size() < 3) {
                sendMessageToClient(clientSocket, "0.0");
            } else {
                auto hull = computeConvexHull(pointsCopy);
                double area = calculatePolygonArea(hull);
                ostringstream out;
                out << fixed << setprecision(1) << area;
                sendMessageToClient(clientSocket, out.str());
            }
            
        } else if (command.substr(0, 9) == "Newpoint ") {
            Point p = parsePointFromString(command.substr(9));
            
            {
                lock_guard<mutex> stateLock(globalStateMutex);
                isGraphLocked = true;
                lockingClientSocket = clientSocket;
                sharedGraphPoints.push_back(p);
                isGraphLocked = false;
                lockingClientSocket = -1;
            }
            
            sendMessageToClient(clientSocket, "Point added");
            processWaitingCommands();
            
        } else if (command.substr(0, 12) == "Removepoint ") {
            Point p = parsePointFromString(command.substr(12));
            bool found = false;
            
            {
                lock_guard<mutex> stateLock(globalStateMutex);
                isGraphLocked = true;
                lockingClientSocket = clientSocket;
                
                for (int i = sharedGraphPoints.size()-1; i >= 0; --i) {
                    if (fabs(sharedGraphPoints[i].x - p.x) < 1e-9 && 
                        fabs(sharedGraphPoints[i].y - p.y) < 1e-9) {
                        sharedGraphPoints.erase(sharedGraphPoints.begin() + i);
                        found = true;
                        break;
                    }
                }
                
                isGraphLocked = false;
                lockingClientSocket = -1;
            }
            
            sendMessageToClient(clientSocket, found ? "Point removed" : "Point not found");
            processWaitingCommands();
            
        } else {
            sendMessageToClient(clientSocket, "Error: Unknown command");
        }
    } catch (const exception& e) {
        sendMessageToClient(clientSocket, "Error: " + string(e.what()));
        
        // If we were in the middle of a graph operation, unlock
        {
            lock_guard<mutex> stateLock(globalStateMutex);
            if (lockingClientSocket == clientSocket) {
                isGraphLocked = false;
                lockingClientSocket = -1;
            }
        }
        processWaitingCommands();
    }
}

void handleClientCommand(int clientSocket, const string& input) {
    cout << "[handleClientCommand] socket=" << clientSocket << ", input=\"" << input << "\"" << endl;

    string command = input;
    while (!command.empty() && isspace(command.back())) command.pop_back();
    if (command.empty()) return;

    try {
        // Check if client is in point-reading mode
        bool inPointMode = false;
        {
            lock_guard<mutex> clientLock(clientDataMutex);
            inPointMode = (clientInputState[clientSocket] == 1);
        }
        
        if (inPointMode) {
            Point p = parsePointFromString(command);
            
            {
                lock_guard<mutex> stateLock(globalStateMutex);
                lock_guard<mutex> clientLock(clientDataMutex);
                
                sharedGraphPoints.push_back(p);
                pointsAlreadyRead[clientSocket]++;
                
                int currentPoints = pointsAlreadyRead[clientSocket];
                int totalPoints = pointsToRead[clientSocket];
                
                sendMessageToClient(clientSocket, "Point " + to_string(currentPoints) + " accepted");

                if (currentPoints >= totalPoints) {
                    sendMessageToClient(clientSocket, 
                        "Graph created with " + to_string(currentPoints) + " points");
                    clientInputState[clientSocket] = 0;
                    isGraphLocked = false;
                    lockingClientSocket = -1;
                }
            }
            
            // Check if we finished reading points
            {
                lock_guard<mutex> clientLock(clientDataMutex);
                if (pointsAlreadyRead[clientSocket] >= pointsToRead[clientSocket]) {
                    processWaitingCommands();
                }
            }
            return;
        }

        // Handle regular commands
        bool needsLock = command == "CH" || command.substr(0,9) == "Newgraph " ||
                        command.substr(0,9) == "Newpoint " || command.substr(0,12) == "Removepoint ";
                      
        if (needsLock) {
            bool shouldQueue = false;
            {
                lock_guard<mutex> stateLock(globalStateMutex);
                shouldQueue = (isGraphLocked && lockingClientSocket != clientSocket);
            }
            
            if (shouldQueue) {
                {
                    lock_guard<mutex> queueLock(commandQueueMutex);
                    waitingCommands.push(PendingCommand(clientSocket, command));
                }
                sendMessageToClient(clientSocket, "Command queued");
                return;
            }
        }

        executeClientCommand(clientSocket, command);
        
    } catch (const exception& e) {
        sendMessageToClient(clientSocket, "Error: " + string(e.what()));
        
        bool inPointMode = false;
        int nextPoint = 0;
        {
            lock_guard<mutex> clientLock(clientDataMutex);
            inPointMode = (clientInputState[clientSocket] == 1);
            nextPoint = pointsAlreadyRead[clientSocket] + 1;
        }
        
        if (inPointMode) {
            sendMessageToClient(clientSocket, 
                "Please enter point " + to_string(nextPoint) + " again (x,y):");
        }
    }
}

void cleanupClient(int clientSocket) {
    cout << "[cleanupClient] Cleaning up client " << clientSocket << endl;
    
    // Release lock if this client holds it
    {
        lock_guard<mutex> stateLock(globalStateMutex);
        if (lockingClientSocket == clientSocket) {
            isGraphLocked = false;
            lockingClientSocket = -1;
        }
    }
    
    // Remove client from all tracking maps
    {
        lock_guard<mutex> clientLock(clientDataMutex);
        clientBuffers.erase(clientSocket);
        clientInputState.erase(clientSocket);
        pointsToRead.erase(clientSocket);
        pointsAlreadyRead.erase(clientSocket);
    }
    
    // Remove client from reactor and close socket
    reactor.removeFd(clientSocket);
    close(clientSocket);
    
    // Process any waiting commands now that this client released resources
    processWaitingCommands();
}

void handleNewConnection(int fd) {
    sockaddr_in addr;
    socklen_t size = sizeof(addr);
    int client = accept(fd, (sockaddr*)&addr, &size);
    if (client < 0) {
        cerr << "[handleNewConnection] Error accepting client: " << strerror(errno) << endl;
        return;
    }

    cout << "[handleNewConnection] New client connected: " << client << endl;
    
    // Set non-blocking mode for client socket
    int flags = fcntl(client, F_GETFL, 0);
    if (flags == -1 || fcntl(client, F_SETFL, flags | O_NONBLOCK) == -1) {
        cerr << "[handleNewConnection] Error setting non-blocking mode: " << strerror(errno) << endl;
        close(client);
        return;
    }
    
    // Initialize client state
    {
        lock_guard<mutex> clientLock(clientDataMutex);
        clientBuffers[client] = "";
        clientInputState[client] = 0;
        pointsToRead[client] = 0;
        pointsAlreadyRead[client] = 0;
    }

    sendMessageToClient(client, "Convex Hull Server Ready");
    sendMessageToClient(client, "Commands: Newgraph n, CH, Newpoint x,y, Removepoint x,y");

    auto clientHandler = [](int fd) {
        char buf[MAX_BUFFER_SIZE];
        ssize_t bytes = recv(fd, buf, sizeof(buf)-1, 0);
        
        if (bytes <= 0) {
            if (bytes == 0) {
                cout << "[clientHandler] Client " << fd << " disconnected normally" << endl;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                cout << "[clientHandler] Client " << fd << " disconnected with error: " 
                     << strerror(errno) << endl;
            } else {
                // EAGAIN/EWOULDBLOCK - no data available, not an error
                return;
            }
            
            cleanupClient(fd);
            return;
        }

        buf[bytes] = '\0';
        string input(buf);
        cout << "[clientHandler] received from fd=" << fd << ": \"" << input << "\"" << endl;

        // Add to client buffer and process complete lines
        {
            lock_guard<mutex> clientLock(clientDataMutex);
            clientBuffers[fd] += input;
        }

        string buffer;
        {
            lock_guard<mutex> clientLock(clientDataMutex);
            buffer = clientBuffers[fd];
        }

        size_t pos;
        while ((pos = buffer.find('\n')) != string::npos) {
            string cmd = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            
            // Update client buffer
            {
                lock_guard<mutex> clientLock(clientDataMutex);
                clientBuffers[fd] = buffer;
            }
            
            // Clean command string
            cmd.erase(0, cmd.find_first_not_of(" \t\r\n"));
            cmd.erase(cmd.find_last_not_of(" \t\r\n") + 1);
            
            if (!cmd.empty()) {
                handleClientCommand(fd, cmd);
            }
            
            // Refresh buffer for next iteration
            {
                lock_guard<mutex> clientLock(clientDataMutex);
                buffer = clientBuffers[fd];
            }
        }
    };

    if (reactor.addFd(client, clientHandler) != 0) {
        cerr << "[handleNewConnection] Failed to add client to reactor" << endl;
        cleanupClient(client);
    }
}

int main() {
    cout << "=== Convex Hull Server with Reactor Pattern ===" << endl;
    
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Error creating server socket: " << strerror(errno) << endl;
        return 1;
    }
    
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Error setting socket options: " << strerror(errno) << endl;
        return 1;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(addr.sin_zero), '\0', 8);

    if (bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "Error binding socket: " << strerror(errno) << endl;
        return 1;
    }
    
    if (listen(serverSocket, 10) < 0) {
        cerr << "Error listening on socket: " << strerror(errno) << endl;
        return 1;
    }

    cout << "Server started on port " << PORT << " using Reactor pattern" << endl;
    cout << "Waiting for connections..." << endl;

    if (reactor.addFd(serverSocket, handleNewConnection) != 0) {
        cerr << "Failed to add server socket to reactor" << endl;
        return 1;
    }
    
    reactor.start();

    // Main loop - keep server running
    while (true) {
        this_thread::sleep_for(chrono::seconds(1));
    }
    
    cout << "Shutting down server..." << endl;
    reactor.stop();
    close(serverSocket);
    return 0;
}