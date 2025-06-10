#include <iostream>
#include <list>
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
 * Andrew's Monotone Chain Convex Hull Algorithm using std::list
 * Returns list of points forming the convex hull in counter-clockwise order
 */
std::list<Point> convex_hull(std::list<Point> points) {
    if (points.size() <= 1) return points;
    
    // Convert to vector for sorting (list sort is less efficient for this case)
    std::vector<Point> point_vec(points.begin(), points.end());
    std::sort(point_vec.begin(), point_vec.end(), compare_points);
    
    // Build lower hull using list
    std::list<Point> hull;
    for (const auto& point : point_vec) {
        // Remove points that create right turn (list advantage: efficient removal)
        while (hull.size() >= 2) {
            auto it = hull.end();
            --it;  // last point
            Point last = *it;
            --it;  // second to last point
            Point second_last = *it;
            
            if (cross_product(second_last, last, point) <= 0) {
                hull.pop_back();  // list advantage: efficient back removal
            } else {
                break;
            }
        }
        hull.push_back(point);  // list advantage: efficient insertion
    }
    
    // Build upper hull
    size_t lower_size = hull.size();
    for (auto it = point_vec.rbegin() + 1; it != point_vec.rend(); ++it) {
        const Point& point = *it;
        
        while (hull.size() >= lower_size + 1) {
            auto hull_it = hull.end();
            --hull_it;  // last point
            Point last = *hull_it;
            --hull_it;  // second to last point
            Point second_last = *hull_it;
            
            if (cross_product(second_last, last, point) <= 0) {
                hull.pop_back();  // list advantage: efficient back removal
            } else {
                break;
            }
        }
        hull.push_back(point);  // list advantage: efficient insertion
    }
    
    // Remove the last point as it's the same as the first
    if (hull.size() > 1) hull.pop_back();
    
    return hull;
}

/**
 * Calculate area of polygon using Shoelace formula
 */
double calculate_area(const std::list<Point>& hull) {
    if (hull.size() < 3) return 0.0;
    
    double area = 0.0;
    
    auto it = hull.begin();
    auto next_it = hull.begin();
    ++next_it;
    
    for (; next_it != hull.end(); ++it, ++next_it) {
        area += it->x * next_it->y;
        area -= next_it->x * it->y;
    }
    
    // Connect last point to first
    auto last = hull.end();
    --last;
    auto first = hull.begin();
    area += last->x * first->y;
    area -= first->x * last->y;
    
    return std::abs(area) / 2.0;
}

/**
 * Main function - Convex Hull Area Calculator with List
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
    
    // Read point coordinates into list
    std::list<Point> points;
    
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
    std::list<Point> hull = convex_hull(points);
    
    // Calculate and display area
    double area = calculate_area(hull);
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Convex Hull Area (list): " << area << std::endl;
    
    return 0;
}