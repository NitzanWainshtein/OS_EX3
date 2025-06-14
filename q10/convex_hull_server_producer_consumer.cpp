/**
 * Q10 - Convex Hull Server with Producer-Consumer Pattern
 * -------------------------------------------------------
 * This server extends Step 9 by adding a consumer thread that waits for
 * convex hull area to reach at least 100 square units.
 * Uses POSIX condition variables to notify the consumer thread.
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
#include <pthread.h>

using namespace std;

#define PORT 9034
#define MAX_BUFFER_SIZE 1024
#define TARGET_AREA 100.0

// Basic Point structure
struct Point {
    double x, y;
    Point() : x(0), y(0) {}
    Point(double x, double y) : x(x), y(y) {}
};

// Global shared resources
vector<Point> sharedGraphPoints;
Proactor globalProactor;
atomic<bool> serverRunning(true);
int serverSocket = -1;

// Producer-Consumer synchronization
pthread_mutex_t areaMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t areaCondition = PTHREAD_COND_INITIALIZER;
double currentArea = 0.0;
bool areaAboveTarget = false;  // Track if we're currently above 100
pthread_t consumerThread;

// Utility functions (same as q9)
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
 * Producer function: Updates the area and notifies consumer
 * This is called whenever a user initiates a CH calculation
 */
void updateAreaAndNotify(double newArea) {
    pthread_mutex_lock(&areaMutex);
    
    double oldArea = currentArea;
    currentArea = newArea;
    bool wasAboveTarget = areaAboveTarget;
    
    cout << "[Producer] Area updated: " << fixed << setprecision(1) << newArea << " units" << endl;
    
    // Check if we crossed the 100 unit threshold
    if (!wasAboveTarget && newArea >= TARGET_AREA) {
        // Crossed from below 100 to above 100
        areaAboveTarget = true;
        cout << "[Producer] Area crossed threshold! Notifying consumer..." << endl;
        pthread_cond_signal(&areaCondition);
    } else if (wasAboveTarget && newArea < TARGET_AREA) {
        // Crossed from above 100 to below 100
        areaAboveTarget = false;
        cout << "[Producer] Area dropped below threshold! Notifying consumer..." << endl;
        pthread_cond_signal(&areaCondition);
    }
    
    pthread_mutex_unlock(&areaMutex);
}

/**
 * Consumer thread function: Waits for area changes and prints notifications
 */
void* consumerThreadFunction(void* arg) {
    cout << "[Consumer] Consumer thread started, waiting for area changes..." << endl;
    
    while (serverRunning) {
        pthread_mutex_lock(&areaMutex);
        
        // Wait for condition change
        pthread_cond_wait(&areaCondition, &areaMutex);
        
        if (!serverRunning) {
            pthread_mutex_unlock(&areaMutex);
            break;
        }
        
        // Check current state and print appropriate message
        if (areaAboveTarget) {
            cout << "\n*** At Least 100 units belongs to CH ***" << endl;
            cout << "[Consumer] Current area: " << fixed << setprecision(1) << currentArea << " units" << endl;
        } else {
            cout << "\n*** At Least 100 units no longer belongs to CH ***" << endl;
            cout << "[Consumer] Current area: " << fixed << setprecision(1) << currentArea << " units" << endl;
        }
        
        pthread_mutex_unlock(&areaMutex);
    }
    
    cout << "[Consumer] Consumer thread ending" << endl;
    return nullptr;
}

/**
 * Client handler function - same as q9 but with producer notifications
 */
void* handleClientWithProactorAndConsumer(int clientSocket) {
    cout << "[Proactor] Client handler started for socket " << clientSocket << endl;
    
    // Send welcome messages
    if (!sendMessageToClient(clientSocket, "Convex Hull Server Ready (Step 10 - Producer-Consumer)")) {
        return nullptr;
    }
    if (!sendMessageToClient(clientSocket, "Commands: Newgraph n, CH, Newpoint x,y, Removepoint x,y, exit")) {
        return nullptr;
    }
    if (!sendMessageToClient(clientSocket, "Note: Server monitors for CH area >= 100 square units")) {
        return nullptr;
    }

    char buffer[MAX_BUFFER_SIZE];
    string accumulatedInput;
    int pointsToRead = 0;
    int pointsRead = 0;
    bool readingPoints = false;

    // Main client communication loop
    while (serverRunning) {
        // Set socket timeout
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            if (bytesRead == 0) {
                cout << "[Client " << clientSocket << "] Disconnected normally" << endl;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                cout << "[Client " << clientSocket << "] Disconnected with error: " << strerror(errno) << endl;
            }
            break;
        }

        buffer[bytesRead] = '\0';
        accumulatedInput += buffer;

        // Process complete commands
        size_t pos;
        while ((pos = accumulatedInput.find('\n')) != string::npos) {
            string command = accumulatedInput.substr(0, pos);
            accumulatedInput.erase(0, pos + 1);

            command.erase(0, command.find_first_not_of(" \t\r\n"));
            command.erase(command.find_last_not_of(" \t\r\n") + 1);
            
            if (command.empty()) continue;

            cout << "[Client " << clientSocket << "] Command: " << command << endl;

            try {
                if (readingPoints) {
                    // Handle point input for Newgraph command
                    Point p = parsePointFromString(command);
                    
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
                        
                        // PRODUCER EVENT: Calculate area after graph creation
                        globalProactor.lockGraphForWrite();
                        vector<Point> points = sharedGraphPoints;
                        globalProactor.unlockGraphForWrite();
                        
                        if (points.size() >= 3) {
                            auto hull = computeConvexHull(points);
                            double area = calculatePolygonArea(hull);
                            updateAreaAndNotify(area);  // Notify consumer
                        } else {
                            updateAreaAndNotify(0.0);   // Notify consumer
                        }
                    }
                    continue;
                }

                // Handle main commands
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
                    
                    // PRODUCER EVENT: Graph cleared
                    updateAreaAndNotify(0.0);
                    
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
                        // PRODUCER EVENT: Area is 0
                        updateAreaAndNotify(0.0);
                    } else {
                        auto hull = computeConvexHull(points);
                        double area = calculatePolygonArea(hull);
                        ostringstream out;
                        out << fixed << setprecision(1) << area;
                        if (!sendMessageToClient(clientSocket, out.str())) {
                            goto client_disconnected;
                        }
                        // PRODUCER EVENT: User initiated CH calculation
                        updateAreaAndNotify(area);
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
                    
                    // NOTE: No automatic area calculation here - only when user requests CH
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
                    
                    // NOTE: No automatic area calculation here - only when user requests CH
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
    return nullptr;
}

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    cout << "\n[Server] Received signal " << signum << ", shutting down gracefully..." << endl;
    serverRunning = false;
    
    // Wake up consumer thread
    pthread_mutex_lock(&areaMutex);
    pthread_cond_signal(&areaCondition);
    pthread_mutex_unlock(&areaMutex);
    
    // Close server socket
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
    
    cout << "=== Step 10: Convex Hull Server with Producer-Consumer Pattern ===" << endl;
    cout << "This server extends Step 9 with a consumer thread that monitors CH area" << endl;
    cout << "Target area: " << TARGET_AREA << " square units" << endl;
    
    // Start consumer thread
    int result = pthread_create(&consumerThread, nullptr, consumerThreadFunction, nullptr);
    if (result != 0) {
        cerr << "Failed to create consumer thread: " << strerror(result) << endl;
        return 1;
    }
    
    // Create server socket
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
    
    cout << "Server started on port " << PORT << " with Producer-Consumer pattern" << endl;
    cout << "Consumer thread monitors for CH area >= " << TARGET_AREA << " units" << endl;
    cout << "Press Ctrl+C to stop the server gracefully" << endl;
    
    // Start the proactor
    pthread_t proactorThread = globalProactor.startProactor(serverSocket, handleClientWithProactorAndConsumer);
    
    if (proactorThread == 0) {
        cerr << "Failed to start proactor" << endl;
        return 1;
    }
    
    cout << "Proactor started with thread ID: " << proactorThread << endl;
    cout << "Consumer thread started, waiting for area changes..." << endl;
    cout << "Waiting for connections..." << endl;
    
    // Main thread waits for shutdown signal
    while (serverRunning) {
        sleep(1);
    }
    
    // Graceful shutdown
    cout << "[Server] Shutting down..." << endl;
    
    // Stop the proactor
    globalProactor.stopProactor(proactorThread);
    
    // Wait for consumer thread to finish
    pthread_join(consumerThread, nullptr);
    
    // Clean up synchronization objects
    pthread_mutex_destroy(&areaMutex);
    pthread_cond_destroy(&areaCondition);
    
    // Close server socket
    if (serverSocket >= 0) {
        close(serverSocket);
    }
    
    cout << "[Server] Step 10 server shutdown complete. Goodbye!" << endl;
    return 0;
}