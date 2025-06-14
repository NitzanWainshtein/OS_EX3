#include "Reactor.hpp"
#include <sys/select.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <errno.h>
#include <chrono>

Reactor::Reactor() : running(false) {}

Reactor::~Reactor() {
    stop();
}

void Reactor::start() {
    if (!running) {
        running = true;
        reactorThread = std::thread(&Reactor::reactorLoop, this);
        std::cout << "[Reactor] Started reactor loop" << std::endl;
    }
}

int Reactor::addFd(int fd, reactorFunc func) {
    if (fd < 0 || !func) {
        std::cerr << "[Reactor] Error: Invalid fd or function" << std::endl;
        return -1;
    }
    
    std::lock_guard<std::mutex> lock(reactorMutex);
    std::cout << "[Reactor] Adding fd " << fd << " to reactor" << std::endl;
    fdFuncMap[fd] = func;
    return 0;
}

int Reactor::removeFd(int fd) {
    std::lock_guard<std::mutex> lock(reactorMutex);
    std::cout << "[Reactor] Removing fd " << fd << " from reactor" << std::endl;
    
    auto it = fdFuncMap.find(fd);
    if (it == fdFuncMap.end()) {
        std::cerr << "[Reactor] Warning: fd " << fd << " not found in reactor" << std::endl;
        return -1;
    }
    
    fdFuncMap.erase(it);
    return 0;
}

int Reactor::stop() {
    if (running) {
        std::cout << "[Reactor] Stopping reactor..." << std::endl;
        running = false;
        
        if (reactorThread.joinable()) {
            reactorThread.join();
        }
        std::cout << "[Reactor] Reactor stopped" << std::endl;
    }
    return 0;
}

bool Reactor::isRunning() const {
    return running;
}

void Reactor::reactorLoop() {
    std::cout << "[Reactor] Reactor loop started" << std::endl;
    
    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxfd = -1;

        // Build the fd_set from our map
        {
            std::lock_guard<std::mutex> lock(reactorMutex);
            
            // If no file descriptors are registered, just sleep and continue
            if (fdFuncMap.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            for (const auto& [fd, func] : fdFuncMap) {
                FD_SET(fd, &readfds);
                if (fd > maxfd) {
                    maxfd = fd;
                }
            }
        }

        // Set timeout to 1 second (reduced CPU usage)
        timeval tv = {1, 0};
        
        int activity = select(maxfd + 1, &readfds, nullptr, nullptr, &tv);
        
        // Handle select errors
        if (activity < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, continue
                continue;
            } else {
                perror("[Reactor] select() error");
                continue;
            }
        }
        
        // Timeout occurred - continue to next iteration
        if (activity == 0) {
            continue;
        }

        // Create a copy of the map to avoid holding the lock during callbacks
        std::map<int, reactorFunc> tmpMap;
        {
            std::lock_guard<std::mutex> lock(reactorMutex);
            tmpMap = fdFuncMap;
        }

        // Check which file descriptors are ready and call their handlers
        for (const auto& [fd, func] : tmpMap) {
            if (FD_ISSET(fd, &readfds)) {
                std::cout << "[Reactor] fd " << fd << " is ready, calling handler" << std::endl;
                
                try {
                    func(fd);
                } catch (const std::exception& e) {
                    std::cerr << "[Reactor] Exception in handler for fd " << fd 
                              << ": " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[Reactor] Unknown exception in handler for fd " << fd << std::endl;
                }
            }
        }
    }
    
    std::cout << "[Reactor] Reactor loop ended" << std::endl;
}