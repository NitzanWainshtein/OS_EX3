#pragma once

#include <functional>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>

/**
 * @brief A simple Reactor design pattern implementation using select().
 * 
 * This class allows you to register file descriptors and corresponding callback functions.
 * When any of the registered file descriptors becomes readable, the associated function is called.
 */
typedef std::function<void(int)> reactorFunc;

class Reactor {
private:
    std::map<int, reactorFunc> fdFuncMap;    ///< Maps file descriptors to their handler functions
    std::atomic<bool> running;               ///< Indicates whether the reactor is currently running
    std::thread reactorThread;               ///< Background thread running the reactor loop
    std::mutex reactorMutex;                 ///< Protects fdFuncMap from concurrent access

    /**
     * @brief The main loop of the reactor.
     * Monitors registered file descriptors and dispatches the appropriate handler on readiness.
     */
    void reactorLoop();

public:
    /**
     * @brief Constructor. Initializes the reactor in stopped state.
     */
    Reactor();

    /**
     * @brief Destructor. Automatically stops the reactor loop.
     */
    ~Reactor();

    /**
     * @brief Starts the reactor loop in a background thread.
     */
    void start();

    /**
     * @brief Adds a file descriptor and its callback function to the reactor.
     * 
     * @param fd    File descriptor to monitor.
     * @param func  Callback function to call when fd is ready for reading.
     * @return int  0 on success, -1 on error.
     */
    int addFd(int fd, reactorFunc func);

    /**
     * @brief Removes a file descriptor from the reactor.
     * 
     * @param fd    File descriptor to remove.
     * @return int  0 on success, -1 on error.
     */
    int removeFd(int fd);

    /**
     * @brief Stops the reactor loop and joins the background thread.
     * 
     * @return int  0 on success.
     */
    int stop();

    /**
     * @brief Check if reactor is currently running.
     * 
     * @return bool true if running, false otherwise.
     */
    bool isRunning() const;
};