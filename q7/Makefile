# Makefile for q7 - Multi-threaded Convex Hull Server

CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2

# Source files
SERVER_SRC = convex_hull_server_threads.cpp
TARGET = convex_hull_server_threads

# Default target
all: $(TARGET)

# Build the multi-threaded server
$(TARGET): $(SERVER_SRC)
	@echo "Compiling Multi-threaded Convex Hull Server..."
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SERVER_SRC)
	@echo "Build successful!"

# Debug build with additional debugging symbols
debug: CXXFLAGS += -g -DDEBUG -fsanitize=thread
debug: $(TARGET)

# Build with address sanitizer for memory leak detection
sanitize: CXXFLAGS += -g -fsanitize=address -fsanitize=leak
sanitize: $(TARGET)

# Run the server
run: $(TARGET)
	@echo "Starting Multi-threaded Convex Hull Server on port 9034..."
	@echo "Each client gets its own dedicated thread"
	@echo "Use 'telnet localhost 9034' or 'nc localhost 9034' to connect"
	@echo "Press Ctrl+C to stop the server gracefully"
	@echo "================================================"
	./$(TARGET)

# Test server with multiple concurrent clients
test-multi: $(TARGET)
	@echo "Starting server in background..."
	./$(TARGET) &
	@SERVER_PID=$$!; \
	sleep 2; \
	echo "Testing server with multiple concurrent clients..."; \
	( echo -e "Newgraph 3\n1,1\n2,2\n3,1\nCH\nexit" | nc localhost 9034 ) & \
	( sleep 1; echo -e "Newpoint 4,4\nCH\nexit" | nc localhost 9034 ) & \
	( sleep 2; echo -e "CH\nexit" | nc localhost 9034 ) & \
	wait; \
	sleep 1; \
	kill $$SERVER_PID 2>/dev/null || true; \
	echo "Multi-client test completed"

# Performance test with many clients
stress-test: $(TARGET)
	@echo "Starting stress test with 10 concurrent clients..."
	./$(TARGET) &
	@SERVER_PID=$$!; \
	sleep 2; \
	for i in {1..10}; do \
		( echo -e "Newgraph 4\n$$i,$$i\n$$((i+1)),$$((i+1))\n$$((i+2)),$$i\n$$i,$$((i+2))\nCH\nexit" | nc localhost 9034 > /dev/null ) & \
	done; \
	wait; \
	sleep 2; \
	kill $$SERVER_PID 2>/dev/null || true; \
	echo "Stress test completed"

# Memory leak check with valgrind
valgrind: $(TARGET)
	@echo "Running server with valgrind (memory leak detection)..."
	@echo "Connect with: telnet localhost 9034"
	@echo "Press Ctrl+C when done testing"
	valgrind --tool=memcheck --leak-check=full --track-origins=yes ./$(TARGET)

# Thread safety check with helgrind
helgrind: $(TARGET)
	@echo "Running server with helgrind (race condition detection)..."
	@echo "This will be slow but will detect thread safety issues"
	valgrind --tool=helgrind ./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET) *.o core

# Clean and rebuild
rebuild: clean all

# Show server status
status:
	@echo "Checking if server is running on port 9034..."
	@if netstat -ln 2>/dev/null | grep -q ":9034 "; then \
		echo "Server is running on port 9034"; \
		echo "Active connections:"; \
		netstat -n 2>/dev/null | grep ":9034" || echo "No active connections"; \
	else \
		echo "Server is not running"; \
	fi

# Kill any running server instances
kill-server:
	@echo "Killing any running server instances..."
	@pkill -f $(TARGET) || echo "No server instances found"

.PHONY: all run debug sanitize test-multi stress-test valgrind helgrind clean rebuild status kill-server help