#pragma once
#include <pthread.h>
#include <map>

typedef void* (*proactorFunc)(int sockfd);

class Proactor {
private:
    pthread_mutex_t graphMutex;
    pthread_mutex_t proactorsMutex;
    std::map<pthread_t, int> activeProactors;
    
    // Internal structure for accept thread
    struct AcceptThreadData {
        int sockfd;
        proactorFunc threadFunc;
        Proactor* proactor;
    };
    
    // Static function for accept thread
    static void* acceptThreadFunction(void* args);

public:
    Proactor() {
        pthread_mutex_init(&graphMutex, nullptr);
        pthread_mutex_init(&proactorsMutex, nullptr);
    }

    ~Proactor() {
        pthread_mutex_destroy(&graphMutex);
        pthread_mutex_destroy(&proactorsMutex);
    }

    /**
     * @brief Starts new proactor and returns proactor thread id.
     * Creates accept loop that spawns threads for each connecting client.
     */
    pthread_t startProactor(int sockfd, proactorFunc threadFunc);
    
    /**
     * @brief Stops proactor by threadid
     */
    int stopProactor(pthread_t tid);

    // Mutex operations for graph modifications
    void lockGraphForWrite() { pthread_mutex_lock(&graphMutex); }
    void unlockGraphForWrite() { pthread_mutex_unlock(&graphMutex); }

    Proactor(const Proactor&) = delete;
    Proactor& operator=(const Proactor&) = delete;
};