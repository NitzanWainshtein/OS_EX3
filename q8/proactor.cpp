#include "proactor.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <fcntl.h>
#include <errno.h>

struct ClientThreadArgs {
    int clientSocket;
    proactorFunc func;
};

// Wrapper function to convert between pthread signature and proactorFunc signature
static void* clientThreadWrapper(void* args) {
    auto* clientArgs = static_cast<ClientThreadArgs*>(args);
    int clientSocket = clientArgs->clientSocket;
    proactorFunc func = clientArgs->func;
    
    // Call the user's function with the socket
    void* result = func(clientSocket);
    
    // Clean up
    close(clientSocket);
    delete clientArgs;
    
    return result;
}

// Static function for accept thread - this is the core of proactor pattern
void* Proactor::acceptThreadFunction(void* args) {
    auto* data = static_cast<AcceptThreadData*>(args);
    int serverSocket = data->sockfd;
    proactorFunc clientFunc = data->threadFunc;
    Proactor* proactor = data->proactor;
    
    std::cout << "[Proactor] Accept thread started on socket " << serverSocket << std::endl;
    
    // Set socket to non-blocking to avoid hanging on accept
    int flags = fcntl(serverSocket, F_GETFL, 0);
    fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);
    
    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        // Accept new connection
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No pending connections, sleep briefly
                usleep(100000); // 100ms
                continue;
            } else {
                std::cerr << "[Proactor] Accept failed: " << strerror(errno) << std::endl;
                break; // Exit on real error
            }
        }
        
        std::cout << "[Proactor] New client connected: " << clientSocket << std::endl;
        
        // Create wrapper args for the client thread
        auto* clientArgs = new ClientThreadArgs{clientSocket, clientFunc};
        
        // Create new thread for this client
        pthread_t clientThread;
        int result = pthread_create(&clientThread, nullptr, clientThreadWrapper, clientArgs);
        
        if (result != 0) {
            std::cerr << "[Proactor] Failed to create client thread: " << strerror(result) << std::endl;
            close(clientSocket);
            delete clientArgs;
            continue;
        }
        
        // Store thread in map
        pthread_mutex_lock(&proactor->proactorsMutex);
        proactor->activeProactors[clientThread] = clientSocket;
        pthread_mutex_unlock(&proactor->proactorsMutex);
        
        // Detach thread for automatic cleanup
        pthread_detach(clientThread);
    }
    
    std::cout << "[Proactor] Accept thread ending" << std::endl;
    delete data;
    return nullptr;
}

pthread_t Proactor::startProactor(int sockfd, proactorFunc threadFunc) {
    std::cout << "[Proactor] Starting proactor on socket " << sockfd << std::endl;
    
    AcceptThreadData* data = new AcceptThreadData{sockfd, threadFunc, this};
    
    pthread_t tid;
    int result = pthread_create(&tid, nullptr, acceptThreadFunction, data);
    
    if (result == 0) {
        pthread_mutex_lock(&proactorsMutex);
        activeProactors[tid] = sockfd;
        pthread_mutex_unlock(&proactorsMutex);
        std::cout << "[Proactor] Proactor started with thread ID " << tid << std::endl;
        return tid;
    }
    
    delete data;
    std::cerr << "[Proactor] Failed to start proactor: " << strerror(result) << std::endl;
    return 0;
}

int Proactor::stopProactor(pthread_t tid) {
    std::cout << "[Proactor] Stopping proactor " << tid << std::endl;
    
    pthread_mutex_lock(&proactorsMutex);
    auto it = activeProactors.find(tid);
    if (it == activeProactors.end()) {
        pthread_mutex_unlock(&proactorsMutex);
        return -1;
    }
    
    int sockfd = it->second;
    activeProactors.erase(it);
    pthread_mutex_unlock(&proactorsMutex);

    pthread_cancel(tid);
    pthread_join(tid, nullptr);
    close(sockfd);
    
    std::cout << "[Proactor] Proactor " << tid << " stopped" << std::endl;
    return 0;
}