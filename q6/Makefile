# Makefile for q6 - Reactor-based Convex Hull Server (step 6)

CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2

# Source files
SERVER_SRC = convex_hull_server_reactor.cpp
REACTOR_SRC = ../q5/Reactor.cpp
TARGET = convex_hull_server_reactor

# Build server (linked to Reactor library from q5)
all: $(TARGET)

$(TARGET): $(SERVER_SRC) $(REACTOR_SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SERVER_SRC) $(REACTOR_SRC)

# Run the server (Ctrl+C to stop)
run: $(TARGET)
	@echo "Running Convex Hull Reactor Server..."
	./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Help message
help:
	@echo "Reactor-based Convex Hull Server - Step 6"
	@echo "----------------------------------------"
	@echo "  make         - Compile the server"
	@echo "  make run     - Run the server"
	@echo "  make clean   - Delete the server executable"
	@echo "  make help    - Show this help message"