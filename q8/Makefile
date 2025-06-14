CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra

# Default target - build proactor library
all: proactor.o

# Compile proactor.cpp to object file
proactor.o: proactor.cpp proactor.hpp
	$(CXX) $(CXXFLAGS) -c proactor.cpp

# Clean build artifacts
clean:
	rm -f *.o


.PHONY: all clean help