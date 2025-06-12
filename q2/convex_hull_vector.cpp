#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iomanip>

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
 * Returns positive if counter-clockwise, negative if clockwise, 0 if collinear
 */
double cross_product(const Point& O, const Point& A, const Point& B) {
    return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

/**
 * Comparison function for sorting points
 * First by x-coordinate, then by y-coordinate
 */
bool compare_points(const Point& a, const Point& b) {
    if (a.x != b.x) 
        return a.x < b.x;
    return a.y < b.y;
}

/**
 * Andrew's Monotone Chain Convex Hull Algorithm
 * Returns vector of points forming the convex hull in counter-clockwise order
 */
std::vector<Point> convex_hull(std::vector<Point> points) {
    int n = points.size();
    if (n <= 1) return points;
    
    // Sort points lexicographically
    std::sort(points.begin(), points.end(), compare_points);
    
    // Build lower hull
    std::vector<Point> hull;
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
double calculate_area(const std::vector<Point>& hull) {
    if (hull.size() < 3) return 0.0;
    
    double area = 0.0;
    int n = hull.size();
    
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += hull[i].x * hull[j].y;
        area -= hull[j].x * hull[i].y;
    }
    
    return std::abs(area) / 2.0;
}

/**
 * Main function - Convex Hull Area Calculator
 * Input: number of points, then x,y coordinates (comma-separated)
 * Output: area of convex hull
 */
int main() {
    std::cout << "Enter number of points: ";
    int num_points;
    
    // Validate input for number of points
    if (!(std::cin >> num_points)) {
        std::cerr << "Error: Invalid input for number of points" << std::endl;
        return 1;
    }
    
    if (num_points <= 0) {
        std::cerr << "Error: Number of points must be positive" << std::endl;
        return 1;
    }
    
    if (num_points < 3) {
        std::cerr << "Error: Need at least 3 points for convex hull" << std::endl;
        return 1;
    }
    
    // Read point coordinates
    std::vector<Point> points;
    points.reserve(num_points);
    
    std::cout << "Enter points in format x,y (one per line):" << std::endl;
    for (int i = 0; i < num_points; i++) {
        double x, y;
        char comma;
        
        // Simple input reading with validation
        if (!(std::cin >> x >> comma >> y) || comma != ',') {
            std::cerr << "Error: Invalid input format for point " << (i + 1) << std::endl;
            return 1;
        }
        
        points.push_back(Point(x, y));
    }
    
    // Compute convex hull
    std::vector<Point> hull = convex_hull(points);
    
    // Calculate and display area
    double area = calculate_area(hull);
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Convex Hull Area: " << area << std::endl;
    
    return 0;
}