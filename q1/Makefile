# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11
LIBS = -lm
DEBUG_FLAGS = -g -O0
COVERAGE_FLAGS = -fprofile-arcs -ftest-coverage

# Target executable
TARGET = convex_hull_cpp
SOURCE = convex_hull.cpp

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# Build with debug symbols for valgrind
debug: $(SOURCE)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# Build with coverage flags
coverage-build: clean
	$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Run with valgrind memory check
valgrind: debug
	@echo "Running Valgrind memory check..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

# Run with valgrind and input file
valgrind-input: debug
	@echo "Running Valgrind with test input..."
	printf "4\n0,0\n0,1\n1,1\n2,0\n" | valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)
	
# Quick test with sample input
test: $(TARGET)
	@echo "Testing with sample input..."
	printf "4\n0,0\n0,1\n1,1\n2,0\n" | ./$(TARGET)

# Test multiple scenarios
test-all: $(TARGET)
	@echo "=== Testing multiple scenarios ==="
	@echo "Test 1: Normal case"
	printf "4\n0,0\n0,1\n1,1\n2,0\n" | ./$(TARGET)
	@echo "\nTest 2: Triangle"
	printf "3\n0,0\n0,1\n0,1\n" | ./$(TARGET)
	@echo "\nTest 3: Error case (too few points)"
	printf "2\n0,0\n1,1\n" | ./$(TARGET) || true

# Clean all generated files
clean:
	rm -f $(TARGET) *.gcda *.gcno *.gcov

# Install dependencies
install-deps:
	sudo apt-get update
	sudo apt-get install valgrind gcc g++ build-essential

.PHONY: all run debug valgrind valgrind-input coverage-build test-coverage coverage-report coverage-full clean install-deps test test-all