/**
 * Step 6 - Convex Hull Server with Reactor (Final Debuggable Version)
 * -------------------------------------------------------------------
 * This server handles multiple clients using the Reactor pattern from step 5.
 * It implements all command logic from step 4: Newgraph, Newpoint, Removepoint, CH.
 * Uses a shared graph, client input states, and command queuing with proper locking.
 * Debug prints added to trace command flow and socket activity.
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

// Global state and per-client tracking
vector<Point> sharedGraphPoints;
bool isGraphLocked = false;
int lockingClientSocket = -1;

map<int, int> clientInputState;      // 0: normal, 1: reading points
map<int, int> pointsToRead;
map<int, int> pointsAlreadyRead;
map<int, string> clientBuffers;
queue<PendingCommand> waitingCommands;

Reactor reactor;
int serverSocket;

// Utilities
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
    send(clientSocket, formatted.c_str(), formatted.size(), 0);
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
    for (const Point& p : pts) {
        while (hull.size() >= 2 && crossProduct(hull[hull.size()-2], hull[hull.size()-1], p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
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

void executeClientCommand(int clientSocket, const string& command);

void processWaitingCommands() {
    while (!waitingCommands.empty() && !isGraphLocked) {
        auto cmd = waitingCommands.front();
        waitingCommands.pop();
        executeClientCommand(cmd.clientSocket, cmd.commandText);
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
            
            isGraphLocked = true;
            lockingClientSocket = clientSocket;
            sharedGraphPoints.clear();
            sendMessageToClient(clientSocket, "Enter " + to_string(n) + " points (x,y):");
            clientInputState[clientSocket] = 1;
            pointsToRead[clientSocket] = n;
            pointsAlreadyRead[clientSocket] = 0;
            
        } else if (command == "CH") {
            if (sharedGraphPoints.size() < 3) {
                sendMessageToClient(clientSocket, "0");
            } else {
                auto hull = computeConvexHull(sharedGraphPoints);
                double area = calculatePolygonArea(hull);
                ostringstream out;
                out << fixed << setprecision(1) << area;
                sendMessageToClient(clientSocket, out.str());
            }
            
        } else if (command.substr(0, 9) == "Newpoint ") {
            Point p = parsePointFromString(command.substr(9));
            isGraphLocked = true;
            lockingClientSocket = clientSocket;
            sharedGraphPoints.push_back(p);
            sendMessageToClient(clientSocket, "Point added");
            isGraphLocked = false;
            lockingClientSocket = -1;
            processWaitingCommands();
            
        } else if (command.substr(0, 12) == "Removepoint ") {
            Point p = parsePointFromString(command.substr(12));
            isGraphLocked = true;
            lockingClientSocket = clientSocket;
            
            bool found = false;
            for (int i = sharedGraphPoints.size()-1; i >= 0; --i) {
                if (fabs(sharedGraphPoints[i].x - p.x) < 1e-9 && fabs(sharedGraphPoints[i].y - p.y) < 1e-9) {
                    sharedGraphPoints.erase(sharedGraphPoints.begin() + i);
                    found = true;
                    break;
                }
            }
            
            sendMessageToClient(clientSocket, found ? "Point removed" : "Point not found");
            isGraphLocked = false;
            lockingClientSocket = -1;
            processWaitingCommands();
            
        } else {
            sendMessageToClient(clientSocket, "Error: Unknown command");
        }
    } catch (const exception& e) {
        sendMessageToClient(clientSocket, "Error: " + string(e.what()));
    }
}

void handleClientCommand(int clientSocket, const string& input) {
    cout << "[handleClientCommand] socket=" << clientSocket << ", input=\"" << input << "\"" << endl;

    string command = input;
    while (!command.empty() && isspace(command.back())) command.pop_back();
    if (command.empty()) return;

    try {
        if (clientInputState[clientSocket] == 1) {
            Point p = parsePointFromString(command);
            sharedGraphPoints.push_back(p);
            pointsAlreadyRead[clientSocket]++;
            sendMessageToClient(clientSocket, "Point " + to_string(pointsAlreadyRead[clientSocket]) + " accepted");

            if (pointsAlreadyRead[clientSocket] >= pointsToRead[clientSocket]) {
                sendMessageToClient(clientSocket, "Graph created with " + to_string(pointsAlreadyRead[clientSocket]) + " points");
                clientInputState[clientSocket] = 0;
                isGraphLocked = false;
                lockingClientSocket = -1;
                processWaitingCommands();
            }
            return;
        }

        bool needsLock = command == "CH" || command.substr(0,9) == "Newgraph " ||
                      command.substr(0,9) == "Newpoint " || command.substr(0,12) == "Removepoint ";
                      
        if (isGraphLocked && needsLock && lockingClientSocket != clientSocket) {
            waitingCommands.push(PendingCommand(clientSocket, command));
            sendMessageToClient(clientSocket, "Command queued");
            return;
        }

        executeClientCommand(clientSocket, command);
        
    } catch (const exception& e) {
        sendMessageToClient(clientSocket, "Error: " + string(e.what()));
        if (clientInputState[clientSocket] == 1) {
            sendMessageToClient(clientSocket, "Please enter point " + 
                to_string(pointsAlreadyRead[clientSocket] + 1) + " again (x,y):");
        }
    }
}

void handleNewConnection(int fd) {
    sockaddr_in addr;
    socklen_t size = sizeof(addr);
    int client = accept(fd, (sockaddr*)&addr, &size);
    if (client < 0) {
        cerr << "Error accepting client connection" << endl;
        return;
    }

    cout << "New client connected: " << client << endl;
    
    // Initialize client state
    clientBuffers[client] = "";
    clientInputState[client] = 0;
    pointsToRead[client] = 0;
    pointsAlreadyRead[client] = 0;

    // הגדרת non-blocking mode עבור הסוקט של הלקוח
    int flags = fcntl(client, F_GETFL, 0);
    fcntl(client, F_SETFL, flags | O_NONBLOCK);

    sendMessageToClient(client, "Convex Hull Server Ready");
    sendMessageToClient(client, "Commands: Newgraph n, CH, Newpoint x,y, Removepoint x,y");

    auto clientHandler = [](int fd) {
        char buf[MAX_BUFFER_SIZE];
        int bytes = recv(fd, buf, sizeof(buf)-1, 0);
        
        if (bytes <= 0) {
            if (bytes == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                cout << "Client " << fd << " disconnected" << endl;
                
                // ניקוי משאבי הלקוח
                if (lockingClientSocket == fd) {
                    isGraphLocked = false;
                    lockingClientSocket = -1;
                    processWaitingCommands();
                }
                
                close(fd);
                reactor.removeFd(fd);
                clientBuffers.erase(fd);
                clientInputState.erase(fd);
                pointsToRead.erase(fd);
                pointsAlreadyRead.erase(fd);
            }
            return;
        }

        buf[bytes] = '\0';
        string input(buf);
        cout << "[lambda] received from fd=" << fd << ": \"" << input << "\"" << endl;

        clientBuffers[fd] += input;

        size_t pos;
        while ((pos = clientBuffers[fd].find('\n')) != string::npos) {
            string cmd = clientBuffers[fd].substr(0, pos);
            clientBuffers[fd].erase(0, pos + 1);
            
            // הסרת whitespace מתחילת וסוף הפקודה
            cmd.erase(0, cmd.find_first_not_of(" \t\r\n"));
            cmd.erase(cmd.find_last_not_of(" \t\r\n") + 1);
            
            if (!cmd.empty()) {
                handleClientCommand(fd, cmd);
            }
        }
    };

    reactor.addFd(client, clientHandler);
}

int main() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(addr.sin_zero), '\0', 8);

    if (bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "Error binding socket" << endl;
        return 1;
    }
    
    if (listen(serverSocket, 10) < 0) {
        cerr << "Error listening on socket" << endl;
        return 1;
    }

    cout << "Server started on port " << PORT << " using Reactor pattern" << endl;

    reactor.addFd(serverSocket, handleNewConnection);
    reactor.start();

    while (true) sleep(1);
    
    close(serverSocket);
    return 0;
}