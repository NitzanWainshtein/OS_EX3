#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <string>
#include <sstream>

using namespace std;

/**
 * Point structure for 2D coordinates
 */
struct Point {
    double x, y;
    
    Point() : x(0), y(0) {}
    Point(double x, double y) : x(x), y(y) {}
};

/**
 * Cross product of vectors OA and OB
 */
double cross_product(const Point& O, const Point& A, const Point& B) {
    return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

/**
 * Comparison function for sorting points
 */
bool compare_points(const Point& a, const Point& b) {
    if (a.x != b.x) 
        return a.x < b.x;
    return a.y < b.y;
}

/**
 * Andrew's Monotone Chain Convex Hull Algorithm
 */
vector<Point> convex_hull(vector<Point> points) {
    int n = points.size();
    if (n <= 1) return points;
    
    // Sort points lexicographically
    sort(points.begin(), points.end(), compare_points);
    
    // Build lower hull
    vector<Point> hull;
    for (int i = 0; i < n; i++) {
        while (hull.size() >= 2 && 
               cross_product(hull[hull.size()-2], hull[hull.size()-1], points[i]) <= 0) {
            hull.pop_back();
        }
        hull.push_back(points[i]);
    }
    
    // Build upper hull
    size_t lower_size = hull.size();
    for (int i = n - 2; i >= 0; i--) {
        while (hull.size() >= lower_size + 1 && 
               cross_product(hull[hull.size()-2], hull[hull.size()-1], points[i]) <= 0) {
            hull.pop_back();
        }
        hull.push_back(points[i]);
    }
    
    // Remove the last point as it's the same as the first
    if (hull.size() > 1) hull.pop_back();
    
    return hull;
}

/**
 * Calculate area of polygon using Shoelace formula
 */
double calculate_area(const vector<Point>& hull) {
    if (hull.size() < 3) return 0.0;
    
    double area = 0.0;
    int n = hull.size();
    
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += hull[i].x * hull[j].y;
        area -= hull[j].x * hull[i].y;
    }
    
    return abs(area) / 2.0;
}

/**
 * Safely parse integer from string
 * Returns true if successful, false otherwise
 */
bool parseInteger(const string& str, int& result) {
    try {
        size_t pos;
        result = stoi(str, &pos);
        // Check if entire string was consumed (no extra characters)
        return pos == str.length();
    } catch (...) {
        return false;
    }
}

/**
 * Safely parse double from string
 * Returns true if successful, false otherwise
 */
bool parseDouble(const string& str, double& result) {
    try {
        size_t pos;
        result = stod(str, &pos);
        // Check if entire string was consumed (no extra characters)
        return pos == str.length();
    } catch (...) {
        return false;
    }
}

/**
 * Parse point from string "x,y"
 * Returns true if successful, false otherwise
 */
bool parsePoint(const string& pointStr, double& x, double& y) {
    // Find comma position
    size_t commaPos = pointStr.find(',');
    
    // Check if comma exists and is not at the beginning or end
    if (commaPos == string::npos || commaPos == 0 || commaPos == pointStr.length() - 1) {
        return false;
    }
    
    // Extract x and y strings
    string x_str = pointStr.substr(0, commaPos);
    string y_str = pointStr.substr(commaPos + 1);
    
    // Remove leading/trailing whitespace
    x_str.erase(0, x_str.find_first_not_of(" \t"));
    x_str.erase(x_str.find_last_not_of(" \t") + 1);
    y_str.erase(0, y_str.find_first_not_of(" \t"));
    y_str.erase(y_str.find_last_not_of(" \t") + 1);
    
    // Check for empty strings
    if (x_str.empty() || y_str.empty()) {
        return false;
    }
    
    // Parse numbers
    return parseDouble(x_str, x) && parseDouble(y_str, y);
}

int main() {
    vector<Point> currentGraph;
    string command;
    
    cout << "Interactive Convex Hull Calculator" << endl;
    cout << "Commands: Newgraph n, CH, Newpoint x,y, Removepoint x,y, Q (quit)" << endl;
    cout << "Enter command: ";
    
    while (getline(cin, command)) {
        
        // Remove whitespace
        command.erase(0, command.find_first_not_of(" \t"));
        command.erase(command.find_last_not_of(" \t") + 1);
        
        if (command.empty()) {
            cout << "Enter command: ";
            continue;
        }
        
        // Quit command
        if (command == "Q" || command == "q") {
            cout << "Goodbye!" << endl;
            currentGraph.clear();
            break;
        }
        
        // Newgraph command
        if (command.length() >= 9 && command.substr(0, 9) == "Newgraph ") {
            
            string numberStr = command.substr(9);
            numberStr.erase(0, numberStr.find_first_not_of(" \t"));
            numberStr.erase(numberStr.find_last_not_of(" \t") + 1);
            
            int num_points;
            if (!parseInteger(numberStr, num_points)) {
                cout << "Error: Invalid number format in Newgraph command" << endl;
                cout << "Enter command: ";
                continue;
            }
            
            if (num_points <= 0) {
                cout << "Error: Number of points must be positive" << endl;
                cout << "Enter command: ";
                continue;
            }
            
            if (num_points > 10000) {
                cout << "Error: Too many points (maximum 10000)" << endl;
                cout << "Enter command: ";
                continue;
            }
            
            currentGraph.clear();
            currentGraph.reserve(num_points);
            
            cout << "Enter " << num_points << " points in format x,y:" << endl;
            
            int pointsRead = 0;
            while (pointsRead < num_points) {
                cout << "Point " << (pointsRead + 1) << "/" << num_points << ": ";
                
                string pointLine;
                if (!getline(cin, pointLine)) {
                    cout << "Error: Unexpected end of input" << endl;
                    break;
                }
                
                double x, y;
                if (parsePoint(pointLine, x, y)) {
                    // Check for duplicate points
                    bool isDuplicate = false;
                    for (const Point& p : currentGraph) {
                        if (abs(p.x - x) < 1e-9 && abs(p.y - y) < 1e-9) {
                            isDuplicate = true;
                            break;
                        }
                    }
                    
                    if (isDuplicate) {
                        cout << "Warning: Point (" << x << "," << y << ") already exists. Please enter a different point." << endl;
                    } else {
                        currentGraph.push_back(Point(x, y));
                        pointsRead++;
                    }
                } else {
                    cout << "Error: Invalid point format: '" << pointLine << "'. Expected format: x,y" << endl;
                    cout << "Please try again." << endl;
                }
            }
            
            cout << "Graph created with " << pointsRead << " points" << endl;
        }
        
        // CH command
        else if (command == "CH") {
            
            if (currentGraph.empty()) {
                cout << "Error: No graph exists. Use Newgraph command first" << endl;
            } else if (currentGraph.size() < 3) {
                cout << "0" << endl;
            } else {
                try {
                    vector<Point> hull = convex_hull(currentGraph);
                    double area = calculate_area(hull);
                    cout << fixed << setprecision(1) << area << endl;
                } catch (...) {
                    cout << "Error: Failed to compute convex hull" << endl;
                }
            }
        }
        
        // Newpoint command
        else if (command.length() >= 9 && command.substr(0, 9) == "Newpoint ") {
            
            if (currentGraph.empty()) {
                cout << "Error: No graph exists. Use Newgraph command first" << endl;
            } else {
                string pointStr = command.substr(9);
                pointStr.erase(0, pointStr.find_first_not_of(" \t"));
                pointStr.erase(pointStr.find_last_not_of(" \t") + 1);
                
                double x, y;
                if (parsePoint(pointStr, x, y)) {
                    // Check for duplicate points
                    bool isDuplicate = false;
                    for (const Point& p : currentGraph) {
                        if (abs(p.x - x) < 1e-9 && abs(p.y - y) < 1e-9) {
                            isDuplicate = true;
                            break;
                        }
                    }
                    
                    if (isDuplicate) {
                        cout << "Warning: Point (" << x << "," << y << ") already exists" << endl;
                    } else {
                        currentGraph.push_back(Point(x, y));
                        cout << "Point (" << x << "," << y << ") added" << endl;
                    }
                } else {
                    cout << "Error: Invalid point format in Newpoint command. Expected format: x,y" << endl;
                }
            }
        }
        
        // Removepoint command
        else if (command.length() >= 12 && command.substr(0, 12) == "Removepoint ") {
            
            if (currentGraph.empty()) {
                cout << "Error: No graph exists. Use Newgraph command first" << endl;
            } else {
                string pointStr = command.substr(12);
                pointStr.erase(0, pointStr.find_first_not_of(" \t"));
                pointStr.erase(pointStr.find_last_not_of(" \t") + 1);
                
                double x, y;
                if (!parsePoint(pointStr, x, y)) {
                    cout << "Error: Invalid point format in Removepoint command. Expected format: x,y" << endl;
                } else {
                    bool found = false;
                    for (int i = currentGraph.size() - 1; i >= 0; i--) {
                        if (abs(currentGraph[i].x - x) < 1e-9 && 
                            abs(currentGraph[i].y - y) < 1e-9) {
                            currentGraph.erase(currentGraph.begin() + i);
                            found = true;
                            break;
                        }
                    }
                    
                    if (found) {
                        cout << "Point (" << x << "," << y << ") removed" << endl;
                    } else {
                        cout << "Point (" << x << "," << y << ") not found" << endl;
                    }
                }
            }
        }
        
        // Unknown command
        else {
            cout << "Error: Unknown command '" << command << "'" << endl;
            cout << "Available commands:" << endl;
            cout << "  Newgraph n       - Create new graph with n points" << endl;
            cout << "  CH               - Calculate convex hull area" << endl;
            cout << "  Newpoint x,y     - Add new point (x,y)" << endl;
            cout << "  Removepoint x,y  - Remove point (x,y)" << endl;
            cout << "  Q or q           - Quit program" << endl;
        }
        
        cout << "Enter command: ";
    }
    
    return 0;
}