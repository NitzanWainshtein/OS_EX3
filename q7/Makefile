CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2

all: convex_hull_server_threads

convex_hull_server_threads: convex_hull_server_threads.cpp
	$(CXX) $(CXXFLAGS) -o convex_hull_server_threads convex_hull_server_threads.cpp

clean:
	rm -f convex_hull_server_threads