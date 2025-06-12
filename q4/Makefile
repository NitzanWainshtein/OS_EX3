CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2

# Target executables
SERVER_TARGET = convex_hull_server
CLIENT_TARGET = convex_hull_client

# Source files
SERVER_SOURCE = convex_hull_server.cpp
CLIENT_SOURCE = convex_hull_client.cpp

# Build both server and client
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Build server executable
$(SERVER_TARGET): $(SERVER_SOURCE)
	$(CXX) $(CXXFLAGS) -o $(SERVER_TARGET) $(SERVER_SOURCE)

# Build client executable
$(CLIENT_TARGET): $(CLIENT_SOURCE)
	$(CXX) $(CXXFLAGS) -o $(CLIENT_TARGET) $(CLIENT_SOURCE)

# Run server
server: $(SERVER_TARGET)
	@echo "Starting Convex Hull Server..."
	./$(SERVER_TARGET)

# Run server (alternative command)
run-server: server

# Run client
client: $(CLIENT_TARGET)
	@echo "Starting Convex Hull Client..."
	./$(CLIENT_TARGET)

# Run client (alternative command)
run-client: client

# Quick demo: start server in background and then client
demo: $(SERVER_TARGET) $(CLIENT_TARGET)
	@echo "Starting server in background..."
	@./$(SERVER_TARGET) &
	@sleep 2
	@echo "Starting client..."
	@./$(CLIENT_TARGET)

# Test with automated commands
test: $(SERVER_TARGET) $(CLIENT_TARGET)
	@echo "Testing server with automated client commands..."
	@./$(SERVER_TARGET) &
	@sleep 1
	@echo -e "Newgraph 3\n0,0\n1,0\n0,1\nCH\nquit" | ./$(CLIENT_TARGET)
	@pkill -f $(SERVER_TARGET) || true

# Clean build artifacts
clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)

# Kill any running server processes
kill-server:
	@pkill -f $(SERVER_TARGET) || echo "No server processes found"

# Show help
help:
	@echo "Available targets:"
	@echo "  all         - Build both server and client"
	@echo "  server      - Run the server"
	@echo "  client      - Run the client"
	@echo "  demo        - Start server and client for demo"
	@echo "  test        - Run automated test"
	@echo "  clean       - Remove build artifacts"
	@echo "  kill-server - Kill running server processes"
	@echo "  help        - Show this help message"

.PHONY: all server client demo test clean kill-server help run-server run-client