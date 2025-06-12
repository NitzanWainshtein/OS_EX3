CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -pg
TARGETS = convex_hull_vector convex_hull_deque convex_hull_list

all: $(TARGETS)

convex_hull_vector: convex_hull_vector.cpp
	$(CXX) $(CXXFLAGS) -o convex_hull_vector convex_hull_vector.cpp

convex_hull_deque: convex_hull_deque.cpp
	$(CXX) $(CXXFLAGS) -o convex_hull_deque convex_hull_deque.cpp

convex_hull_list: convex_hull_list.cpp
	$(CXX) $(CXXFLAGS) -o convex_hull_list convex_hull_list.cpp

# Run performance comparison
profile: $(TARGETS)
	@echo "=== Performance Comparison - Convex Hull Implementations ==="
	@echo "Generated on: $$(date)"
	@echo ""
	
	@echo "Creating test input (100,000 points)..."
	@python3 -c "import random; f=open('input.txt','w'); f.write('100000\\n'); [f.write(f'{random.randint(-10000,10000)},{random.randint(-10000,10000)}\\n') for _ in range(100000)]; f.close()"
	
	@echo ""
	@echo "--- Testing std::vector implementation ---"
	@time ./convex_hull_vector < input.txt > /dev/null
	@gprof convex_hull_vector gmon.out > profile_vector.txt 2>/dev/null || echo "No profiling data"
	@if [ -f gmon.out ]; then mv gmon.out gmon_vector.out; fi
	
	@echo ""
	@echo "--- Testing std::deque implementation ---"
	@time ./convex_hull_deque < input.txt > /dev/null
	@gprof convex_hull_deque gmon.out > profile_deque.txt 2>/dev/null || echo "No profiling data"
	@if [ -f gmon.out ]; then mv gmon.out gmon_deque.out; fi
	
	@echo ""
	@echo "--- Testing std::list implementation ---"
	@time ./convex_hull_list < input.txt > /dev/null
	@gprof convex_hull_list gmon.out > profile_list.txt 2>/dev/null || echo "No profiling data"
	@if [ -f gmon.out ]; then mv gmon.out gmon_list.out; fi
	
	@echo ""
	@echo "=== Results Summary ==="
	@echo "Detailed profiling files generated:"
	@echo "  - profile_vector.txt"
	@echo "  - profile_deque.txt"
	@echo "  - profile_list.txt"
	@echo ""
	@echo "See README.md for analysis and conclusions."

# Test correctness
test: $(TARGETS)
	@echo "=== Correctness Test ==="
	@echo "Vector result:"
	@printf "4\n0,0\n0,1\n1,1\n2,0\n" | ./convex_hull_vector
	@echo "Deque result:"
	@printf "4\n0,0\n0,1\n1,1\n2,0\n" | ./convex_hull_deque
	@echo "List result:"
	@printf "4\n0,0\n0,1\n1,1\n2,0\n" | ./convex_hull_list

clean:
	rm -f $(TARGETS) *.txt *.out

.PHONY: all profile test clean