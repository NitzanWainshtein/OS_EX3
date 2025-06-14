CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra

# Default target
all: proactor.o

# Compile proactor.cpp to object file
proactor.o: proactor.cpp proactor.hpp
	$(CXX) $(CXXFLAGS) -c proactor.cpp

clean:
	rm -f *.o