#include "Reactor.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <atomic>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>

// Global flag for clean shutdown
std::atomic<bool> running(true);
Reactor* globalReactor = nullptr;

void signalHandler(int signum) {
    std::cout << "\n[Main] Received signal " << signum << ", shutting down gracefully..." << std::endl;
    running = false;
    
    if (globalReactor) {
        globalReactor->stop();
    }
}

void onInput(int fd) {
    static std::string inputBuffer;
    char buffer[1024];
    
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        inputBuffer += buffer;
        
        // Process complete lines
        size_t pos;
        while ((pos = inputBuffer.find('\n')) != std::string::npos) {
            std::string line = inputBuffer.substr(0, pos);
            inputBuffer.erase(0, pos + 1);
            
            // Remove carriage return if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            if (!line.empty()) {
                std::cout << "[Input Handler] Received: \"" << line << "\"" << std::endl;
                
                // Check for exit command
                if (line == "exit" || line == "quit") {
                    std::cout << "[Input Handler] Exit command received, stopping..." << std::endl;
                    running = false;
                    if (globalReactor) {
                        globalReactor->stop();
                    }
                    return;
                }
            }
        }
    } else if (bytes == 0) {
        std::cout << "[Input Handler] EOF received, stopping..." << std::endl;
        running = false;
        if (globalReactor) {
            globalReactor->stop();
        }
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("[Input Handler] read() error");
        }
    }
}

int main() {
    std::cout << "=== Reactor Pattern Demo ===" << std::endl;
    std::cout << "Type anything and press Enter (type 'exit' or 'quit' to stop, or press CTRL+C)" << std::endl;
    std::cout << "=============================\n" << std::endl;

    // Set up signal handlers for clean shutdown
    signal(SIGINT, signalHandler);   // CTRL+C
    signal(SIGTERM, signalHandler);  // Termination signal

    // Create reactor instance
    Reactor reactor;
    globalReactor = &reactor;

    // Add stdin to reactor
    if (reactor.addFd(STDIN_FILENO, onInput) != 0) {
        std::cerr << "Failed to add stdin to reactor" << std::endl;
        return 1;
    }

    // Start the reactor
    reactor.start();

    // Main loop - wait until shutdown is requested
    while (running && reactor.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n[Main] Cleaning up..." << std::endl;
    
    // Stop reactor if not already stopped
    reactor.stop();
    
    std::cout << "[Main] Goodbye!" << std::endl;
    return 0;
}