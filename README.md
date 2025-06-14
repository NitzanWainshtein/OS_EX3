# OS_EX3 - Convex Hull Project

## Authors
- **Nitzan Wainshtein**, ID: 209086263
- **Aviv Oz**, ID: 207543927

## Project Overview

This project implements a comprehensive convex hull calculation system, progressing from a basic algorithm to a sophisticated multi-threaded server with various design patterns. The project demonstrates advanced concepts in network programming, multi-threading, synchronization, and design patterns.

## Project Structure

```
convex-hull-project/
├── q1/     - Basic convex hull implementation
├── q2/     - Performance comparison of data structures
├── q3/     - Interactive convex hull calculator
├── q4/     - Multi-client server (single-threaded)
├── q5/     - Reactor pattern library
├── q6/     - Server using Reactor pattern
├── q7/     - Multi-threaded server
├── q8/     - Proactor pattern library
├── q9/     - Server using Proactor pattern
├── q10/    - Producer-Consumer pattern server
├── Makefile - Root build system
└── README.md
```

## Build Instructions

### Build Everything
```bash
make all
```

### Build Specific Step
```bash
make q5    # Build step 5
make q7    # Build step 7
```

### Clean Project
```bash
make clean
```

## Step-by-Step Description

### Step 1: Basic Convex Hull (q1/)
- **Objective**: Implement Andrew's Monotone Chain algorithm
- **Input**: Number of points, then x,y coordinates
- **Output**: Area of convex hull
- **Key Features**: Input validation, precise floating-point calculations

### Step 2: Performance Analysis (q2/)
- **Objective**: Compare performance of different data structures
- **Implementations**: `std::vector`, `std::deque`, `std::list`
- **Analysis**: Execution time, memory usage, cache efficiency
- **Result**: `std::vector` performs best for convex hull operations

### Step 3: Interactive Calculator (q3/)
- **Objective**: Add interactive command-line interface
- **Commands**:
  - `Newgraph n` - Create new graph with n points
  - `CH` - Calculate convex hull area
  - `Newpoint x,y` - Add point
  - `Removepoint x,y` - Remove point
- **Features**: Robust input parsing, error handling

### Step 4: Multi-Client Server (q4/)
- **Objective**: Network server supporting multiple clients
- **Protocol**: Text-based over TCP port 9034
- **Architecture**: Single-threaded with command queuing
- **Features**: Shared graph state, client state management

### Step 5: Reactor Pattern Library (q5/)
- **Objective**: Implement Reactor design pattern
- **Key Components**:
  - File descriptor monitoring with `select()`
  - Callback-based event handling
  - Thread-safe operations
- **API**: `addFd()`, `removeFd()`, `start()`, `stop()`

### Step 6: Reactor-Based Server (q6/)
- **Objective**: Rebuild step 4 using Reactor pattern
- **Benefits**: Cleaner event-driven architecture
- **Features**: Non-blocking I/O, scalable client handling

### Step 7: Multi-Threaded Server (q7/)
- **Objective**: Thread-per-client architecture
- **Architecture**: Main accept thread + N client threads
- **Synchronization**: Mutex protection for shared graph
- **Features**: Thread-safe operations, graceful shutdown

### Step 8: Proactor Pattern Library (q8/)
- **Objective**: Implement Proactor design pattern
- **Key Features**:
  - Automatic thread creation for new connections
  - Built-in synchronization mechanisms
- **API**: `startProactor()`, `stopProactor()`

### Step 9: Proactor-Based Server (q9/)
- **Objective**: Rebuild step 7 using Proactor pattern
- **Benefits**: Simplified thread management
- **Architecture**: Proactor handles all threading automatically

### Step 10: Producer-Consumer Pattern (q10/)
- **Objective**: Add monitoring thread for convex hull area
- **Features**:
  - Consumer thread monitors area ≥ 100 square units
  - POSIX condition variables for synchronization
  - Automatic notifications for threshold crossing
- **Messages**:
  - `"At Least 100 units belongs to CH"`
  - `"At Least 100 units no longer belongs to CH"`

## Running the Servers

### Step 4 Server
```bash
make run-q4-server
# Connect with: telnet localhost 9034
```

### Step 7 Multi-threaded Server
```bash
make run-q7-server
# Supports multiple concurrent clients
```

### Step 10 Producer-Consumer Server
```bash
make run-q10-server
# Monitors convex hull area automatically
```

## Protocol Specification

All servers use a text-based protocol over TCP port 9034:

### Commands
- `Newgraph n` - Create graph with n points (followed by n coordinate inputs)
- `CH` - Calculate and return convex hull area
- `Newpoint x,y` - Add point to current graph
- `Removepoint x,y` - Remove point from current graph

### Example Session
```
> Newgraph 4
> 0,0
> 0,1
> 1,1
> 2,0
< Graph created with 4 points
> CH
< 1.5
```