# Makefile for q5 - Reactor Pattern Implementation

CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2

# Source files
REACTOR_SRC = Reactor.cpp
MAIN_SRC = main.cpp
TARGET = reactor_demo

# Headers
HEADERS = Reactor.hpp

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(MAIN_SRC) $(REACTOR_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(MAIN_SRC) $(REACTOR_SRC)

# Build only the reactor object file (for linking with other projects)
reactor.o: $(REACTOR_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $(REACTOR_SRC)

# Run the demo
run: $(TARGET)
	@echo "Starting Reactor demo..."
	@echo "Type anything and press Enter, or 'exit'/'quit' to stop"
	./$(TARGET)

# Debug build with additional debugging symbols
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET) *.o

# Install reactor library (copy headers and compiled library)
install: reactor.o
	@mkdir -p ../lib ../include
	cp $(HEADERS) ../include/
	cp reactor.o ../lib/
	@echo "Reactor library installed in ../lib/ and ../include/"

# Help target
help:
	@echo "Reactor Pattern Implementation - Available targets:"
	@echo "  make           - Build the reactor demo"
	@echo "  make run       - Build and run the demo"
	@echo "  make debug     - Build with debug symbols"
	@echo "  make reactor.o - Build only the reactor object file"
	@echo "  make install   - Install reactor library for other projects"
	@echo "  make clean     - Remove build artifacts"
	@echo "  make help      - Show this help"

.PHONY: all run debug clean install help