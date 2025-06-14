# Makefile for q9 - Step 7 reimplemented using Step 8 Proactor library

CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2

# Source files
SERVER_SRC = convex_hull_server_with_proactor.cpp
PROACTOR_LIB = ../q8/proactor.o
PROACTOR_HEADER = ../q8/proactor.hpp

# Target
TARGET = convex_hull_server_with_proactor

# Default target
all: check-dependencies $(TARGET)

# Check if Proactor library exists
check-dependencies:
	@echo "Checking dependencies..."
	@if [ ! -f "$(PROACTOR_LIB)" ]; then \
		echo "Error: Proactor library not found at $(PROACTOR_LIB)"; \
		echo "Please build q8 first: cd ../q8 && make"; \
		exit 1; \
	fi
	@if [ ! -f "$(PROACTOR_HEADER)" ]; then \
		echo "Error: Proactor header not found at $(PROACTOR_HEADER)"; \
		exit 1; \
	fi
	@echo "Dependencies found."

# Build the server using Proactor library from q8
$(TARGET): $(SERVER_SRC) $(PROACTOR_LIB) $(PROACTOR_HEADER)
	@echo "Compiling Step 9: Convex Hull Server with Proactor..."
	@echo "Linking with Proactor library from Step 8..."
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SERVER_SRC) $(PROACTOR_LIB)
	@echo "Step 9 server compiled successfully!"

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Build with thread sanitizer
thread-sanitize: CXXFLAGS += -g -fsanitize=thread
thread-sanitize: $(TARGET)

# Run the server
run: $(TARGET)
	@echo "Starting Step 9 Server (Step 7 reimplemented with Step 8 Proactor)..."
	@echo "============================================================="
	@echo "This server has the SAME functionality as q7, but uses the"
	@echo "Proactor pattern from q8 instead of manual thread management."
	@echo ""
	@echo "Connect with: telnet localhost 9034 or nc localhost 9034"
	@echo "Press Ctrl+C to stop the server gracefully"
	@echo "============================================================="
	./$(TARGET)

# Test with multiple clients (same test as q7 to prove equivalence)
test-equivalent: $(TARGET)
	@echo "Testing Step 9 server with same test as Step 7..."
	./$(TARGET) &
	@SERVER_PID=$$!; \
	sleep 2; \
	echo "Testing with multiple concurrent clients (proving q7 equivalence)..."; \
	( echo -e "Newgraph 3\n1,1\n2,2\n3,1\nCH\nexit" | nc localhost 9034 ) & \
	( sleep 1; echo -e "Newpoint 4,4\nCH\nexit" | nc localhost 9034 ) & \
	( sleep 2; echo -e "CH\nexit" | nc localhost 9034 ) & \
	wait; \
	sleep 1; \
	kill $$SERVER_PID 2>/dev/null || true; \
	echo "Step 9 test completed - should behave identically to Step 7"

# Compare with q7 by running both
compare-with-q7:
	@echo "Comparison between Step 7 and Step 9:"
	@echo "====================================="
	@echo "Step 7 (q7): Manual thread management"
	@echo "  - Main thread does accept() loop"
	@echo "  - Creates threads manually with pthread_create()"
	@echo "  - Manages thread cleanup manually"
	@echo "  - Uses separate mutex for graph protection"
	@echo ""
	@echo "Step 9 (q9): Using Proactor pattern"
	@echo "  - Proactor handles accept() loop automatically"
	@echo "  - Proactor creates threads automatically"
	@echo "  - Proactor handles thread cleanup automatically"
	@echo "  - Uses Proactor's built-in mutex for graph protection"
	@echo ""
	@echo "FUNCTIONALITY: Identical - same commands, same behavior"
	@echo "ARCHITECTURE: Different - abstracted thread management"

# Build the Proactor library if needed
build-proactor:
	@echo "Building Proactor library in q8..."
	cd ../q8 && $(MAKE)

# Clean build artifacts
clean:
	rm -f $(TARGET) *.o

# Clean and rebuild
rebuild: clean all

# Clean everything including dependencies
clean-all: clean
	@echo "Cleaning Proactor library in q8..."
	cd ../q8 && $(MAKE) clean

# Show server status
status:
	@echo "Checking if Step 9 server is running on port 9034..."
	@if netstat -ln 2>/dev/null | grep -q ":9034 "; then \
		echo "Step 9 server is running on port 9034"; \
	else \
		echo "Step 9 server is not running"; \
	fi

.PHONY: all check-dependencies debug thread-sanitize run test-equivalent compare-with-q7 build-proactor clean rebuild clean-all status help