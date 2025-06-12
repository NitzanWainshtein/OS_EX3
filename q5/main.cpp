#include "Reactor.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

void onInput(int fd) {
    char buffer[1024];
    int bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        std::cout << "Input: " << buffer;
    }
}

int main() {
    Reactor r;
    r.addFd(STDIN_FILENO, onInput);
    r.start();

    std::cout << "Type something (CTRL+C to exit):" << std::endl;
    while (true) {
        sleep(1);
    }

    return 0;
}