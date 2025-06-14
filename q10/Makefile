# Makefile for q10 - Producer-Consumer Pattern

CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2

# Source and dependencies
SERVER_SRC = convex_hull_server_producer_consumer.cpp
PROACTOR_LIB = ../q8/proactor.o
TARGET = convex_hull_server_producer_consumer

# Default target
all: $(TARGET)

# Build the server
$(TARGET): $(SERVER_SRC) $(PROACTOR_LIB)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SERVER_SRC) $(PROACTOR_LIB)

# Run the server
run: $(TARGET)
	@echo "Starting Producer-Consumer Convex Hull Server..."
	@echo "Watch for: 'At Least 100 units belongs to CH'"
	./$(TARGET)

# Clean
clean:
	rm -f $(TARGET)

# Help
help:
	@echo "Step 10: Producer-Consumer Pattern"
	@echo "make     - Build server"
	@echo "make run - Run server" 
	@echo "make clean - Clean files"

.PHONY: all run clean help