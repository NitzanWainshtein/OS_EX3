#include "Reactor.hpp"
#include <sys/select.h>
#include <unistd.h>
#include <iostream>
#include <map>

Reactor::Reactor() : running(false) {}

Reactor::~Reactor() {
    stop();
}

void Reactor::start() {
    if (!running) {
        running = true;
        reactorThread = std::thread(&Reactor::reactorLoop, this);
    }
}

int Reactor::addFd(int fd, reactorFunc func) {
    if (fd < 0 || !func) return -1;
    std::lock_guard<std::mutex> lock(reactorMutex);
    std::cout << "[Reactor] addFd(" << fd << ")" << std::endl;
    fdFuncMap[fd] = func;
    return 0;
}

int Reactor::removeFd(int fd) {
    std::lock_guard<std::mutex> lock(reactorMutex);
    std::cout << "[Reactor] removeFd(" << fd << ")" << std::endl;
    if (fdFuncMap.count(fd) == 0) return -1;
    fdFuncMap.erase(fd);
    return 0;
}

int Reactor::stop() {
    if (running) {
        running = false;
        if (reactorThread.joinable()) {
            reactorThread.join();
        }
    }
    return 0;
}

void Reactor::reactorLoop() {
    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxfd = 0;

        {
            std::lock_guard<std::mutex> lock(reactorMutex);
            for (std::map<int, reactorFunc>::const_iterator it = fdFuncMap.begin(); it != fdFuncMap.end(); ++it) {
                FD_SET(it->first, &readfds);
                if (it->first > maxfd) maxfd = it->first;
            }
        }

        timeval tv = {1, 0};
        int activity = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        std::cout << "[Reactor] select returned " << activity << std::endl;

        if (activity < 0) {
            perror("select");
            continue;
        }

        std::lock_guard<std::mutex> lock(reactorMutex);
        for (std::map<int, reactorFunc>::const_iterator it = fdFuncMap.begin(); it != fdFuncMap.end(); ++it) {
            int fd = it->first;
            if (FD_ISSET(fd, &readfds)) {
                std::cout << "[Reactor] fd " << fd << " is ready, calling handler" << std::endl;
                it->second(fd);
            }
        }
    }
}