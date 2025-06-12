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
#include <algorithm>
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
    size_t comma = pointString.find(',');
    return Point(stod(pointString.substr(0, comma)), stod(pointString.substr(comma + 1)));
}

void sendMessageToClient(int clientSocket, const string& msg) {
    string formatted = msg + "\n";
    send(clientSocket, formatted.c_str(), formatted.size(), 0);
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
    double area = 0;
    for (int i = 0; i < static_cast<int>(poly.size()); i++) {
        int j = (i + 1) % poly.size();
        area += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
    }
    return fabs(area) / 2.0;
}

void executeClientCommand(int clientSocket, const string& command);

void processWaitingCommands() {
    while (!waitingCommands.empty() && !isGraphLocked) {
        auto cmd = waitingCommands.front(); waitingCommands.pop();
        executeClientCommand(cmd.clientSocket, cmd.commandText);
    }
}

void executeClientCommand(int clientSocket, const string& command) {
    cout << "[executeClientCommand] socket=" << clientSocket << ", command='" << command << "'" << endl;

    if (command.substr(0, 9) == "Newgraph ") {
        isGraphLocked = true;
        lockingClientSocket = clientSocket;
        int n = stoi(command.substr(9));
        sharedGraphPoints.clear();
        sendMessageToClient(clientSocket, "Enter " + to_string(n) + " points (x,y):");
        clientInputState[clientSocket] = 1;
        pointsToRead[clientSocket] = n;
        pointsAlreadyRead[clientSocket] = 0;
    } else if (command == "CH") {
        if (sharedGraphPoints.size() < 3) sendMessageToClient(clientSocket, "0");
        else {
            auto hull = computeConvexHull(sharedGraphPoints);
            double area = calculatePolygonArea(hull);
            ostringstream out;
            out << fixed << setprecision(1) << area;
            sendMessageToClient(clientSocket, out.str());
        }
    } else if (command.substr(0, 9) == "Newpoint ") {
        isGraphLocked = true;
        lockingClientSocket = clientSocket;
        Point p = parsePointFromString(command.substr(9));
        sharedGraphPoints.push_back(p);
        sendMessageToClient(clientSocket, "Point added");
        isGraphLocked = false;
        lockingClientSocket = -1;
        processWaitingCommands();
    } else if (command.substr(0, 12) == "Removepoint ") {
        isGraphLocked = true;
        lockingClientSocket = clientSocket;
        Point p = parsePointFromString(command.substr(12));
        for (int i = sharedGraphPoints.size()-1; i >= 0; --i) {
            if (fabs(sharedGraphPoints[i].x - p.x) < 1e-9 && fabs(sharedGraphPoints[i].y - p.y) < 1e-9) {
                sharedGraphPoints.erase(sharedGraphPoints.begin() + i);
                break;
            }
        }
        sendMessageToClient(clientSocket, "Point removed");
        isGraphLocked = false;
        lockingClientSocket = -1;
        processWaitingCommands();
    }
}

void handleClientCommand(int clientSocket, const string& input) {
    cout << "[handleClientCommand] socket=" << clientSocket << ", input=\"" << input << "\"" << endl;

    string command = input;
    while (!command.empty() && isspace(command.back())) command.pop_back();
    if (command.empty()) return;

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
    if (isGraphLocked && needsLock) {
        waitingCommands.push(PendingCommand(clientSocket, command));
        sendMessageToClient(clientSocket, "Command queued");
        return;
    }

    executeClientCommand(clientSocket, command);
}

void handleNewConnection(int fd) {
    sockaddr_in addr;
    socklen_t size = sizeof(addr);
    int client = accept(fd, (sockaddr*)&addr, &size);
    if (client < 0) return;

    cout << "New client connected: " << client << endl;
    sendMessageToClient(client, "Convex Hull Server Ready");
    sendMessageToClient(client, "Commands: Newgraph n, CH, Newpoint x,y, Removepoint x,y");

    clientBuffers[client] = "";

    // ✅ תיקון: שימוש ב-[=] ו־fd מה־Reactor ולא בלכידה לפי ערך
    reactor.addFd(client, [](int fd) {
        char buf[MAX_BUFFER_SIZE];
        int bytes = recv(fd, buf, sizeof(buf)-1, 0);
        if (bytes <= 0) {
            cout << "Client " << fd << " disconnected" << endl;
            close(fd);
            reactor.removeFd(fd);
            clientBuffers.erase(fd);
            return;
        }

        buf[bytes] = '\0';
        string input(buf);
        cout << "[lambda] received command from fd=" << fd << ": \"" << input << "\"" << endl;

        clientBuffers[fd] += input;

        size_t pos;
        while ((pos = clientBuffers[fd].find('\n')) != string::npos) {
            string cmd = clientBuffers[fd].substr(0, pos);
            clientBuffers[fd].erase(0, pos + 1);
            handleClientCommand(fd, cmd);
        }
    });
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

    bind(serverSocket, (sockaddr*)&addr, sizeof(addr));
    listen(serverSocket, 10);

    cout << "Server started on port " << PORT << " using Reactor pattern" << endl;

    reactor.addFd(serverSocket, handleNewConnection);
    reactor.start();

    while (true) sleep(1);
    close(serverSocket);
    return 0;
}