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
        int maxfd = -1;  // שינוי מ-0 ל--1

        {
            std::lock_guard<std::mutex> lock(reactorMutex);
            for (const auto& [fd, func] : fdFuncMap) {
                FD_SET(fd, &readfds);
                if (fd > maxfd) maxfd = fd;
            }
        }

        if (maxfd == -1) {  // אם אין file descriptors פעילים
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        timeval tv = {0, 100000};  // timeout של 100ms במקום שנייה
        int activity = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        
        if (activity < 0) {
            if (errno != EINTR) {  // אם זו לא הפרעה מכוונת
                perror("select");
            }
            continue;
        }

        std::map<int, reactorFunc> tmpMap;
        {
            std::lock_guard<std::mutex> lock(reactorMutex);
            tmpMap = fdFuncMap;  // העתקת ה-map כדי לשחרר את הנעילה
        }

        for (const auto& [fd, func] : tmpMap) {
            if (FD_ISSET(fd, &readfds)) {
                std::cout << "[Reactor] fd " << fd << " is ready, calling handler" << std::endl;
                try {
                    func(fd);
                } catch (const std::exception& e) {
                    std::cerr << "[Reactor] Error in handler for fd " << fd << ": " << e.what() << std::endl;
                }
            }
        }
    }
}