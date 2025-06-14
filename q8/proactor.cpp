#include "proactor.hpp"
#include <iostream>
#include <unistd.h>

struct ThreadArgs {
    int sockfd;
    proactorFunc func;
    Proactor* proactor;
};

static void* threadWrapper(void* args) {
    auto* threadArgs = static_cast<ThreadArgs*>(args);
    
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, nullptr);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
    
    // Execute the function without locking the entire execution
    threadArgs->func(threadArgs->sockfd);
    
    delete threadArgs;
    return nullptr;
}

pthread_t Proactor::startProactor(int sockfd, proactorFunc threadFunc) {
    ThreadArgs* args = new ThreadArgs{sockfd, threadFunc, this};
    
    pthread_t tid;
    int result = pthread_create(&tid, nullptr, threadWrapper, args);
    
    if (result == 0) {
        pthread_mutex_lock(&proactorsMutex);
        activeProactors[tid] = sockfd;
        pthread_mutex_unlock(&proactorsMutex);
        return tid;
    }
    
    delete args;
    return 0;
}

int Proactor::stopProactor(pthread_t tid) {
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
    return 0;
}