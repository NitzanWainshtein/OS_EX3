CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGET = convex_hull_interactive
SOURCE = convex_hull_interactive.cpp

# Build the executable
$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

# Alternative target name
all: $(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Clean generated files
clean:
	rm -f $(TARGET)


.PHONY: all run test clean help