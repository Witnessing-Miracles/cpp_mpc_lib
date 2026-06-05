#pragma once
#include "TaskQueue.hpp"

// Threadpool structure
struct ThreadPool
{
public:
// 提供一些API函数
    /**
    * Constructor function
    * Parameters: maximun and minimum number of worker threads.
    */
    ThreadPool(int min, int max);

    // Destructor function, destroy the pool
    ~ThreadPool();

    // Add tasks into threadpool
    void addTask(Task task);

    // Acquire the number of working threads in pool
    int getBusyNum();

    // Acquire the number of alive threads in pool
    int getAliveNum();

private:
    // Worker thread task function
    static void* worker(void* arg);
    static void* manager(void* arg);
    void threadExit();

private:
    // Task Queue
    TaskQueue* taskQ;

    // Manager thread ID
    pthread_t managerID;

    // Worker threads' id
    pthread_t *threadIDs;

    // Max/Min number of threads in pool
    int minNum;
    int maxNum;

    // Number of working threads
    int busyNum;

    // Number of alive threads currently
    int liveNum;

    /**
     * How many threads need to exit.
     * For example, in this scenario where the number of inventory threads is much greater than
     * the number of worker threads.
     */
    int exitNum;

    /**
     * The data in the task queue is shared and needs to be protected.
     * Therefore, thread synchronization is required.
     */
    pthread_mutex_t mutexPool;          // lock the whole threadpool

    // Thread pool switch: true -> close threadpool, false -> keep it alive
    bool shutdown;

    // Create a condition variable to determine whether the queue is empty.
    pthread_cond_t notEmpty;

    static const int NUMBER = 2;
};