# Makefile for q6 - Reactor-based Convex Hull Server (step 6)

CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2

# Source files
SERVER_SRC = convex_hull_server_reactor.cpp
REACTOR_SRC = ../q5/Reactor.cpp
TARGET = convex_hull_server_reactor

# Headers
REACTOR_HEADER = ../q5/Reactor.hpp

# Default target
all: $(TARGET)

# Build server (linked to Reactor library from q5)
$(TARGET): $(SERVER_SRC) $(REACTOR_SRC) $(REACTOR_HEADER)
	@echo "Compiling Reactor-based Convex Hull Server..."
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SERVER_SRC) $(REACTOR_SRC)
	@echo "Build successful!"

# Debug build with additional debugging symbols
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Run the server (Ctrl+C to stop)
run: $(TARGET)
	@echo "Starting Convex Hull Reactor Server on port 9034..."
	@echo "Use 'telnet localhost 9034' or 'nc localhost 9034' to connect"
	@echo "Press Ctrl+C to stop the server"
	@echo "================================================"
	./$(TARGET)

# Test with a simple client connection
test: $(TARGET)
	@echo "Starting server in background..."
	./$(TARGET) &
	@SERVER_PID=$$!; \
	sleep 2; \
	echo "Testing server with sample commands..."; \
	echo -e "Newgraph 3\n1,1\n2,2\n3,1\nCH\nexit" | nc localhost 9034; \
	kill $$SERVER_PID 2>/dev/null || true

# Check if reactor library is available
check-reactor:
	@if [ ! -f "$(REACTOR_SRC)" ]; then \
		echo "Error: Reactor source not found at $(REACTOR_SRC)"; \
		echo "Please make sure q5 is built first: cd ../q5 && make"; \
		exit 1; \
	fi
	@if [ ! -f "$(REACTOR_HEADER)" ]; then \
		echo "Error: Reactor header not found at $(REACTOR_HEADER)"; \
		exit 1; \
	fi
	@echo "Reactor library found."

# Clean build artifacts
clean:
	rm -f $(TARGET) *.o

# Clean and rebuild
rebuild: clean all

# Show server status (if running)
status:
	@echo "Checking if server is running on port 9034..."
	@if netstat -ln 2>/dev/null | grep -q ":9034 "; then \
		echo "Server is running on port 9034"; \
	else \
		echo "Server is not running"; \
	fi

.PHONY: all run debug test check-reactor clean rebuild status help