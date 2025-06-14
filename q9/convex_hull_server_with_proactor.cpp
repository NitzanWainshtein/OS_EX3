/**
 * Q9 - Convex Hull Server using Proactor Pattern (Step 7 reimplemented with Step 8)
 * ---------------------------------------------------------------------------------
 * This server reimplements the multi-threaded server from step 7 using the 
 * Proactor library created in step 8. Instead of manually managing threads,
 * we use the Proactor pattern to handle accept() and thread creation.
 */

#include "../q8/proactor.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <atomic>

using namespace std;

#define PORT 9034
#define MAX_BUFFER_SIZE 1024

// Basic Point structure
struct Point {
    double x, y;
    Point() : x(0), y(0) {}
    Point(double x, double y) : x(x), y(y) {}
};

// Global shared resources (same as q7, but now protected by Proactor's mutex)
vector<Point> sharedGraphPoints;
Proactor globalProactor;
atomic<bool> serverRunning(true);
int serverSocket = -1;

// Utility functions (identical to q7)
Point parsePointFromString(const string& pointString) {
    try {
        size_t comma = pointString.find(',');
        if (comma == string::npos) {
            throw invalid_argument("Invalid point format: missing comma");
        }
        
        string xStr = pointString.substr(0, comma);
        string yStr = pointString.substr(comma + 1);
        
        xStr.erase(remove_if(xStr.begin(), xStr.end(), ::isspace), xStr.end());
        yStr.erase(remove_if(yStr.begin(), yStr.end(), ::isspace), yStr.end());
        
        if (xStr.empty() || yStr.empty()) {
            throw invalid_argument("Invalid point format: empty coordinate");
        }
        
        return Point(stod(xStr), stod(yStr));
    } catch (const exception& e) {
        throw invalid_argument("Invalid point format: " + string(e.what()));
    }
}

double crossProduct(const Point& o, const Point& a, const Point& b) {
    return (a.x - o.x)*(b.y - o.y) - (a.y - o.y)*(b.x - o.x);
}

vector<Point> computeConvexHull(const vector<Point>& points) {
    if (points.size() <= 1) return points;
    
    vector<Point> pts = points;
    sort(pts.begin(), pts.end(), [](const Point& a, const Point& b) {
        return (a.x != b.x) ? a.x < b.x : a.y < b.y;
    });

    vector<Point> hull;
    // Build lower hull
    for (const Point& p : pts) {
        while (hull.size() >= 2 && 
               crossProduct(hull[hull.size()-2], hull[hull.size()-1], p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
    
    // Build upper hull
    size_t lower = hull.size();
    for (int i = pts.size()-2; i >= 0; i--) {
        while (hull.size() > lower && 
               crossProduct(hull[hull.size()-2], hull[hull.size()-1], pts[i]) <= 0)
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

bool sendMessageToClient(int clientSocket, const string& msg) {
    string formatted = msg + "\n";
    ssize_t sent = send(clientSocket, formatted.c_str(), formatted.length(), MSG_NOSIGNAL);
    if (sent < 0) {
        cout << "[Client " << clientSocket << "] Error sending message: " << strerror(errno) << endl;
        return false;
    }
    cout << "[Client " << clientSocket << "] Sent: " << msg << endl;
    return true;
}

/**
 * Client handler function - this is called by the Proactor for each new client
 * This function replaces the handleClient function from q7, but uses Proactor's 
 * thread management instead of manual thread creation.
 * 
 * KEY DIFFERENCE FROM Q7: This function signature matches proactorFunc typedef:
 * void* (*proactorFunc)(int sockfd) - returns void*, takes int sockfd
 */
void* handleClientWithProactor(int clientSocket) {
    cout << "[Proactor] Client handler started for socket " << clientSocket << endl;
    
    // Send welcome messages (same as q7)
    if (!sendMessageToClient(clientSocket, "Convex Hull Server Ready (Step 9 - Proactor Version)")) {
        return nullptr;
    }
    if (!sendMessageToClient(clientSocket, "Commands: Newgraph n, CH, Newpoint x,y, Removepoint x,y, exit")) {
        return nullptr;
    }

    char buffer[MAX_BUFFER_SIZE];
    string accumulatedInput;
    int pointsToRead = 0;
    int pointsRead = 0;
    bool readingPoints = false;

    // Main client communication loop (same logic as q7)
    while (serverRunning) {
        // Set socket timeout to check serverRunning periodically
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            if (bytesRead == 0) {
                cout << "[Client " << clientSocket << "] Disconnected normally" << endl;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Timeout, check serverRunning and continue
            } else {
                cout << "[Client " << clientSocket << "] Disconnected with error: " << strerror(errno) << endl;
            }
            break;
        }

        buffer[bytesRead] = '\0';
        accumulatedInput += buffer;

        // Process complete commands (same logic as q7)
        size_t pos;
        while ((pos = accumulatedInput.find('\n')) != string::npos) {
            string command = accumulatedInput.substr(0, pos);
            accumulatedInput.erase(0, pos + 1);

            // Clean command string
            command.erase(0, command.find_first_not_of(" \t\r\n"));
            command.erase(command.find_last_not_of(" \t\r\n") + 1);
            
            if (command.empty()) continue;

            cout << "[Client " << clientSocket << "] Command: " << command << endl;

            try {
                if (readingPoints) {
                    // Handle point input for Newgraph command
                    Point p = parsePointFromString(command);
                    
                    // KEY DIFFERENCE: Use Proactor's mutex instead of separate graphMutex
                    globalProactor.lockGraphForWrite();
                    sharedGraphPoints.push_back(p);
                    globalProactor.unlockGraphForWrite();
                    
                    pointsRead++;
                    if (!sendMessageToClient(clientSocket, "Point " + to_string(pointsRead) + " accepted")) {
                        goto client_disconnected;
                    }

                    if (pointsRead >= pointsToRead) {
                        readingPoints = false;
                        if (!sendMessageToClient(clientSocket, 
                            "Graph created with " + to_string(pointsRead) + " points")) {
                            goto client_disconnected;
                        }
                    }
                    continue;
                }

                // Handle main commands (same logic as q7, but with Proactor mutex)
                if (command.substr(0, 9) == "Newgraph ") {
                    try {
                        pointsToRead = stoi(command.substr(9));
                        if (pointsToRead <= 0) {
                            throw invalid_argument("Number of points must be positive");
                        }
                    } catch (const exception& e) {
                        if (!sendMessageToClient(clientSocket, "Error: Invalid number of points")) {
                            goto client_disconnected;
                        }
                        continue;
                    }

                    globalProactor.lockGraphForWrite();
                    sharedGraphPoints.clear();
                    globalProactor.unlockGraphForWrite();
                    
                    pointsRead = 0;
                    readingPoints = true;
                    if (!sendMessageToClient(clientSocket, "Enter " + to_string(pointsToRead) + " points (x,y):")) {
                        goto client_disconnected;
                    }
                }
                else if (command == "CH") {
                    vector<Point> points;
                    
                    globalProactor.lockGraphForWrite();
                    points = sharedGraphPoints;
                    globalProactor.unlockGraphForWrite();
                    
                    if (points.size() < 3) {
                        if (!sendMessageToClient(clientSocket, "0.0")) {
                            goto client_disconnected;
                        }
                    } else {
                        auto hull = computeConvexHull(points);
                        double area = calculatePolygonArea(hull);
                        ostringstream out;
                        out << fixed << setprecision(1) << area;
                        if (!sendMessageToClient(clientSocket, out.str())) {
                            goto client_disconnected;
                        }
                    }
                }
                else if (command.substr(0, 9) == "Newpoint ") {
                    Point p = parsePointFromString(command.substr(9));
                    
                    globalProactor.lockGraphForWrite();
                    sharedGraphPoints.push_back(p);
                    globalProactor.unlockGraphForWrite();
                    
                    if (!sendMessageToClient(clientSocket, "Point added")) {
                        goto client_disconnected;
                    }
                }
                else if (command.substr(0, 12) == "Removepoint ") {
                    Point p = parsePointFromString(command.substr(12));
                    bool found = false;
                    
                    globalProactor.lockGraphForWrite();
                    for (auto it = sharedGraphPoints.begin(); it != sharedGraphPoints.end(); ++it) {
                        if (fabs(it->x - p.x) < 1e-9 && fabs(it->y - p.y) < 1e-9) {
                            sharedGraphPoints.erase(it);
                            found = true;
                            break;
                        }
                    }
                    globalProactor.unlockGraphForWrite();
                    
                    if (!sendMessageToClient(clientSocket, found ? "Point removed" : "Point not found")) {
                        goto client_disconnected;
                    }
                }
                else if (command == "exit" || command == "quit") {
                    sendMessageToClient(clientSocket, "Goodbye!");
                    break;
                }
                else {
                    if (!sendMessageToClient(clientSocket, "Error: Unknown command")) {
                        goto client_disconnected;
                    }
                }
            }
            catch (const exception& e) {
                string errorMsg = "Error: " + string(e.what());
                if (!sendMessageToClient(clientSocket, errorMsg)) {
                    goto client_disconnected;
                }
            }
        }
    }

client_disconnected:
    cout << "[Proactor] Client handler ending for socket " << clientSocket << endl;
    
    // NOTE: Socket cleanup is handled by the Proactor library, not manually here
    // This is a key difference from q7 where we manually managed socket cleanup
    
    return nullptr; // Return required by proactorFunc signature
}

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    cout << "\n[Server] Received signal " << signum << ", shutting down gracefully..." << endl;
    serverRunning = false;
    
    // Close server socket to break accept loop
    if (serverSocket >= 0) {
        shutdown(serverSocket, SHUT_RDWR);
        close(serverSocket);
        serverSocket = -1;
    }
}

int main() {
    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    cout << "=== Step 9: Convex Hull Server using Proactor Library ===" << endl;
    cout << "This server reimplements Step 7 using the Proactor pattern from Step 8" << endl;
    
    // Create server socket (same as q7)
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Error creating socket: " << strerror(errno) << endl;
        return 1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Error setting socket options: " << strerror(errno) << endl;
        return 1;
    }
    
    // Configure server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    memset(&(serverAddr.sin_zero), '\0', 8);
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Error binding socket: " << strerror(errno) << endl;
        return 1;
    }
    
    if (listen(serverSocket, 10) < 0) {
        cerr << "Error listening on socket: " << strerror(errno) << endl;
        return 1;
    }
    
    cout << "Server started on port " << PORT << " using Step 8 Proactor library" << endl;
    cout << "KEY DIFFERENCE FROM Q7: Using Proactor pattern instead of manual thread management" << endl;
    cout << "Press Ctrl+C to stop the server gracefully" << endl;
    
    // KEY DIFFERENCE FROM Q7: Instead of manual accept loop and thread creation,
    // we use the Proactor pattern to handle everything automatically
    pthread_t proactorThread = globalProactor.startProactor(serverSocket, handleClientWithProactor);
    
    if (proactorThread == 0) {
        cerr << "Failed to start proactor" << endl;
        return 1;
    }
    
    cout << "Proactor started with thread ID: " << proactorThread << endl;
    cout << "Waiting for connections..." << endl;
    
    // Main thread waits for shutdown signal (same as q7 concept, but simpler)
    while (serverRunning) {
        sleep(1);
    }
    
    // Graceful shutdown using Proactor
    cout << "[Server] Shutting down..." << endl;
    
    // Stop the proactor (this handles all thread cleanup automatically)
    globalProactor.stopProactor(proactorThread);
    
    // Close server socket
    if (serverSocket >= 0) {
        close(serverSocket);
    }
    
    cout << "[Server] Step 9 server shutdown complete. Goodbye!" << endl;
    return 0;
}