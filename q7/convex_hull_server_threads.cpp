/**
 * Q7 - Multi-threaded Convex Hull Server
 * 
 * This server implements a thread per client model for handling convex hull calculations.
 * Each client gets its own thread, with shared graph access protected by mutex.
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

using namespace std;

#define PORT 9034
#define MAX_BUFFER_SIZE 1024

// Basic Point structure
struct Point {
    double x, y;
    Point() : x(0), y(0) {}
    Point(double x, double y) : x(x), y(y) {}
};

// Shared resources protected by mutex
vector<Point> sharedGraphPoints;
mutex graphMutex;
map<int, thread> clientThreads;

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

// Send formatted message to client with logging
void sendMessageToClient(int clientSocket, const string& msg) {
    string formatted = msg + "\n";
    send(clientSocket, formatted.c_str(), formatted.length(), 0);
    cout << "[Client " << clientSocket << "] Sent: " << msg << endl;
}

/**
 * Main client handler - runs in dedicated thread
 * Handles all communication with a single client
 */
void handleClient(int clientSocket) {
    cout << "[Client " << clientSocket << "] Thread started" << endl;
    
    // Initial client setup
    sendMessageToClient(clientSocket, "Convex Hull Server Ready");
    sendMessageToClient(clientSocket, "Commands: Newgraph n, CH, Newpoint x,y, Removepoint x,y");

    char buffer[MAX_BUFFER_SIZE];
    string accumulatedInput;
    int pointsToRead = 0;
    int pointsRead = 0;
    bool readingPoints = false;

    // Main client communication loop
    while (true) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            cout << "[Client " << clientSocket << "] Disconnected" << endl;
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
                    sendMessageToClient(clientSocket, "Point " + to_string(pointsRead) + " accepted");

                    if (pointsRead >= pointsToRead) {
                        readingPoints = false;
                        sendMessageToClient(clientSocket, 
                            "Graph created with " + to_string(pointsRead) + " points");
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
                        sendMessageToClient(clientSocket, "Error: Invalid number of points");
                        continue;
                    }

                    {
                        lock_guard<mutex> lock(graphMutex);
                        sharedGraphPoints.clear();
                    }
                    pointsRead = 0;
                    readingPoints = true;
                    sendMessageToClient(clientSocket, "Enter " + to_string(pointsToRead) + " points (x,y):");
                }
                else if (command == "CH") {
                    vector<Point> points;
                    {
                        lock_guard<mutex> lock(graphMutex);
                        points = sharedGraphPoints;
                    }
                    
                    if (points.size() < 3) {
                        sendMessageToClient(clientSocket, "0.0");
                    } else {
                        auto hull = computeConvexHull(points);
                        double area = calculatePolygonArea(hull);
                        ostringstream out;
                        out << fixed << setprecision(1) << area;
                        sendMessageToClient(clientSocket, out.str());
                    }
                }
                else if (command.substr(0, 9) == "Newpoint ") {
                    Point p = parsePointFromString(command.substr(9));
                    {
                        lock_guard<mutex> lock(graphMutex);
                        sharedGraphPoints.push_back(p);
                    }
                    sendMessageToClient(clientSocket, "Point added");
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
                    sendMessageToClient(clientSocket, found ? "Point removed" : "Point not found");
                }
                else {
                    sendMessageToClient(clientSocket, "Error: Unknown command");
                }
            }
            catch (const exception& e) {
                sendMessageToClient(clientSocket, "Error: " + string(e.what()));
            }
        }
    }

    close(clientSocket);
}

int main() {
    // Server setup
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Error creating socket" << endl;
        return 1;
    }

    // Set socket options
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configure server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    memset(&(serverAddr.sin_zero), '\0', 8);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Error binding socket" << endl;
        return 1;
    }

    if (listen(serverSocket, 10) < 0) {
        cerr << "Error listening on socket" << endl;
        return 1;
    }

    cout << "Server started on port " << PORT << " (Multi-threaded version)" << endl;

    // Main accept loop
    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0) {
            cerr << "Error accepting connection" << endl;
            continue;
        }

        // Create and detach new client thread
        clientThreads[clientSocket] = thread(handleClient, clientSocket);
        clientThreads[clientSocket].detach();
    }

    close(serverSocket);
    return 0;
}