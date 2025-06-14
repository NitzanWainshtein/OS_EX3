# Root Makefile for Convex Hull Project
# OS Course Assignment 3 - Synchronization and Convex Hull Server

# Project directories
DIRS = q1 q2 q3 q4 q5 q6 q7 q8 q9 q10

# Default target - build all
all:
	@echo "Building all project components..."
	@for dir in $(DIRS); do \
		echo "Building $$dir..."; \
		$(MAKE) -C $$dir --no-print-directory || exit 1; \
	done
	@echo "All components built successfully!"

# Clean all directories
clean:
	@echo "Cleaning all project components..."
	@for dir in $(DIRS); do \
		echo "Cleaning $$dir..."; \
		$(MAKE) -C $$dir clean --no-print-directory 2>/dev/null || true; \
	done
	@echo "All components cleaned!"

# Test all components that have test targets
test:
	@echo "Running tests where available..."
	@for dir in q1 q2 q4 q7; do \
		if [ -f $$dir/Makefile ]; then \
			echo "Testing $$dir..."; \
			$(MAKE) -C $$dir test --no-print-directory 2>/dev/null || true; \
		fi; \
	done

# Build specific step
q%:
	@echo "Building step $@..."
	@$(MAKE) -C $@ --no-print-directory

# Run specific servers
run-q4-server:
	@$(MAKE) -C q4 server

run-q6-server:
	@$(MAKE) -C q6 run

run-q7-server:
	@$(MAKE) -C q7 run

run-q9-server:
	@$(MAKE) -C q9 run

run-q10-server:
	@$(MAKE) -C q10 run

# Show project status
status:
	@echo "=== Convex Hull Project Status ==="
	@for dir in $(DIRS); do \
		echo -n "$$dir: "; \
		if [ -f $$dir/Makefile ]; then \
			echo "Ready"; \
		else \
			echo "Missing Makefile"; \
		fi; \
	done

.PHONY: all clean test status help $(DIRS) run-q4-server run-q6-server run-q7-server run-q9-server run-q10-server