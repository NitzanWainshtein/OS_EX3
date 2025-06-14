/**
 * Q7 - Multi-threaded Convex Hull Server (Thread-Safe Fixed Version)
 * ------------------------------------------------------------------
 * This server implements a thread per client model for handling convex hull calculations.
 * Each client gets its own thread, with shared graph access protected by mutex.
 * Proper thread management, cleanup, and graceful shutdown implemented.
 */

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <algorithm>
#include <functional>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <fcntl.h>
#include <signal.h>

using namespace std;

#define PORT 9034
#define MAX_BUFFER_SIZE 1024

// Basic Point structure
struct Point {
    double x, y;
    Point() : x(0), y(0) {}
    Point(double x, double y) : x(x), y(y) {}
};

// Thread management structure
struct ClientThread {
    thread clientThread;
    atomic<bool> isActive;
    int clientSocket;
    
    ClientThread(int socket) : isActive(true), clientSocket(socket) {}
};

// Global shared resources protected by mutexes
vector<Point> sharedGraphPoints;
mutex graphMutex;                    // Protects the shared graph
map<int, unique_ptr<ClientThread>> clientThreads;
mutex threadMapMutex;                // Protects clientThreads map
atomic<bool> serverRunning(true);    // Server shutdown flag

// Cleanup management
mutex cleanupMutex;
condition_variable cleanupCondition;
thread cleanupThread;

// Function declarations
void cleanupFinishedThreads();
void cleanupClient(int clientSocket);
void handleClient(int clientSocket);
void signalHandler(int signum);

// Utility functions
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

// Calculate cross product for convex hull algorithm
double crossProduct(const Point& o, const Point& a, const Point& b) {
    return (a.x - o.x)*(b.y - o.y) - (a.y - o.y)*(b.x - o.x);
}

// Compute convex hull using Graham's scan algorithm
vector<Point> computeConvexHull(const vector<Point>& points) {
    if (points.size() <= 1) return points;
    
    vector<Point> pts = points;  // Create a copy to sort
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

// Calculate area of polygon using shoelace formula
double calculatePolygonArea(const vector<Point>& poly) {
    if (poly.size() < 3) return 0.0;
    
    double area = 0;
    for (size_t i = 0; i < poly.size(); i++) {
        size_t j = (i + 1) % poly.size();
        area += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
    }
    return fabs(area) / 2.0;
}

// Send formatted message to client with error checking
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

// Clean up finished threads periodically
void cleanupFinishedThreads() {
    while (serverRunning) {
        {
            unique_lock<mutex> lock(cleanupMutex);
            cleanupCondition.wait_for(lock, chrono::seconds(5)); // Check every 5 seconds
        }
        
        vector<int> socketsToClean;
        {
            lock_guard<mutex> lock(threadMapMutex);
            for (auto it = clientThreads.begin(); it != clientThreads.end();) {
                if (!it->second->isActive && it->second->clientThread.joinable()) {
                    cout << "[CleanupThread] Joining finished thread for client " << it->first << endl;
                    it->second->clientThread.join();
                    it = clientThreads.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    cout << "[CleanupThread] Cleanup thread terminated" << endl;
}

// Clean up specific client
void cleanupClient(int clientSocket) {
    cout << "[CleanupClient] Cleaning up client " << clientSocket << endl;
    
    {
        lock_guard<mutex> lock(threadMapMutex);
        auto it = clientThreads.find(clientSocket);
        if (it != clientThreads.end()) {
            it->second->isActive = false;
            // Don't erase here - let cleanup thread handle it
        }
    }
    
    close(clientSocket);
    cleanupCondition.notify_one(); // Wake up cleanup thread
}

/**
 * Main client handler - runs in dedicated thread
 * Handles all communication with a single client
 */
void handleClient(int clientSocket) {
    cout << "[Client " << clientSocket << "] Thread started" << endl;
    
    // Initial client setup
    if (!sendMessageToClient(clientSocket, "Convex Hull Server Ready")) {
        cleanupClient(clientSocket);
        return;
    }
    if (!sendMessageToClient(clientSocket, "Commands: Newgraph n, CH, Newpoint x,y, Removepoint x,y")) {
        cleanupClient(clientSocket);
        return;
    }

    char buffer[MAX_BUFFER_SIZE];
    string accumulatedInput;
    int pointsToRead = 0;
    int pointsRead = 0;
    bool readingPoints = false;

    // Main client communication loop
    while (serverRunning) {
        // Set socket timeout for recv to check serverRunning periodically
        struct timeval tv;
        tv.tv_sec = 1;   // 1 second timeout
        tv.tv_usec = 0;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            if (bytesRead == 0) {
                cout << "[Client " << clientSocket << "] Disconnected normally" << endl;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout occurred, check if server is still running
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

            // Clean command string
            command.erase(0, command.find_first_not_of(" \t\r\n"));
            command.erase(command.find_last_not_of(" \t\r\n") + 1);
            
            if (command.empty()) continue;

            cout << "[Client " << clientSocket << "] Command: " << command << endl;

            try {
                if (readingPoints) {
                    // Handle point input for Newgraph command
                    Point p = parsePointFromString(command);
                    {
                        lock_guard<mutex> lock(graphMutex);
                        sharedGraphPoints.push_back(p);
                    }
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

                    {
                        lock_guard<mutex> lock(graphMutex);
                        sharedGraphPoints.clear();
                    }
                    pointsRead = 0;
                    readingPoints = true;
                    if (!sendMessageToClient(clientSocket, "Enter " + to_string(pointsToRead) + " points (x,y):")) {
                        goto client_disconnected;
                    }
                }
                else if (command == "CH") {
                    vector<Point> points;
                    {
                        lock_guard<mutex> lock(graphMutex);
                        points = sharedGraphPoints;
                    }
                    
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
                    {
                        lock_guard<mutex> lock(graphMutex);
                        sharedGraphPoints.push_back(p);
                    }
                    if (!sendMessageToClient(clientSocket, "Point added")) {
                        goto client_disconnected;
                    }
                }
                else if (command.substr(0, 12) == "Removepoint ") {
                    Point p = parsePointFromString(command.substr(12));
                    bool found = false;
                    {
                        lock_guard<mutex> lock(graphMutex);
                        for (auto it = sharedGraphPoints.begin(); it != sharedGraphPoints.end(); ++it) {
                            if (fabs(it->x - p.x) < 1e-9 && fabs(it->y - p.y) < 1e-9) {
                                sharedGraphPoints.erase(it);
                                found = true;
                                break;
                            }
                        }
                    }
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
    cout << "[Client " << clientSocket << "] Handler ending" << endl;
    cleanupClient(clientSocket);
}

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    cout << "\n[Server] Received signal " << signum << ", shutting down gracefully..." << endl;
    serverRunning = false;
    
    // Close all client sockets to wake up threads
    {
        lock_guard<mutex> lock(threadMapMutex);
        for (auto& [socket, clientThread] : clientThreads) {
            if (clientThread->isActive) {
                cout << "[Server] Closing client socket " << socket << endl;
                shutdown(socket, SHUT_RDWR);
            }
        }
    }
    
    // Wake up cleanup thread
    cleanupCondition.notify_all();
}

int main() {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);   // CTRL+C
    signal(SIGTERM, signalHandler);  // Termination signal
    
    cout << "=== Multi-threaded Convex Hull Server ===" << endl;
    
    // Server setup
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
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

    cout << "Server started on port " << PORT << " (Multi-threaded version)" << endl;
    cout << "Waiting for connections... (Press Ctrl+C to stop)" << endl;

    // Start cleanup thread
    cleanupThread = thread(cleanupFinishedThreads);

    // Set server socket to non-blocking for graceful shutdown
    int flags = fcntl(serverSocket, F_GETFL, 0);
    fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);

    // Main accept loop
    while (serverRunning) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No pending connections, sleep briefly and continue
                this_thread::sleep_for(chrono::milliseconds(100));
                continue;
            } else if (serverRunning) {  // Only print error if we're not shutting down
                cerr << "Error accepting connection: " << strerror(errno) << endl;
            }
            continue;
        }

        cout << "[Server] New client connected: " << clientSocket << endl;

        // Create new client thread with proper management
        {
            lock_guard<mutex> lock(threadMapMutex);
            auto clientThreadObj = make_unique<ClientThread>(clientSocket);
            clientThreadObj->clientThread = thread(handleClient, clientSocket);
            clientThreads[clientSocket] = move(clientThreadObj);
        }
    }

    // Graceful shutdown
    cout << "[Server] Shutting down..." << endl;
    
    // Stop accepting new connections
    close(serverSocket);
    
    // Wait for all client threads to finish
    cout << "[Server] Waiting for client threads to finish..." << endl;
    {
        lock_guard<mutex> lock(threadMapMutex);
        for (auto& [socket, clientThread] : clientThreads) {
            clientThread->isActive = false;
            if (clientThread->clientThread.joinable()) {
                cout << "[Server] Joining thread for client " << socket << endl;
                clientThread->clientThread.join();
            }
        }
        clientThreads.clear();
    }
    
    // Stop cleanup thread
    cleanupCondition.notify_all();
    if (cleanupThread.joinable()) {
        cleanupThread.join();
    }
    
    cout << "[Server] All threads terminated. Goodbye!" << endl;
    return 0;
}