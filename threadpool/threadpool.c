#include "threadpool.h"
#include <thread.h>

// Task structure
typedef struct Task
{
    void (*function)(void* arg);
    void* arg;
} Task;

// Threadpool structure
struct ThreadPool
{
    // Task Queue
    Task* taskQ;

    // Task Queue's capacity
    int queueCapacity;

    // Currently, how many tasks stored in queue
    int queueSize;

    // Queue head and tail
    int queueFront;         // Producer put tasks into front-end
    int queueRear;          // Consumer get tasks from rear

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

    /**
     * The number of busy threads changes frequently,
     * whenever a worker thread finishes its task or retrieves a new task from the queue.
     * Therefore, a lock is added to it.
     */
    pthread_mutex_t mutexBusy;          // Compared with 'mutexpool', 'mutexBusy' is more efficient.

    // Thread pool switch: 1 -> close threadpool, 0 -> keep it alive
    int shutdown;

    // Create a condition variable to determine whether the queue is full or empty.
    pthread_cond_t notFull;
    pthread_cond_t notEmpty;
};

ThreadPool *threadPoolCreate(int min, int max, int queueSize)
{
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do {
        if (pool == NULL)
        {
            printf("malloc threadpool failed ...\n");
            break;
        }

        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
        if (pool->threadIDs == NULL)
        {
            printf("malloc threadIDs failed ...\n");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;
        pool->exitNum = 0;

        if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
            pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
            pthread_cond_init(&pool->notFull, NULL) != 0 ||
            pthread_cond_init(&pool->notEmpty, NULL) != 0)
        {
            printf("mutex or condition init fail ... \n");
            break;
        }

        // Task queue
        pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;
        pool->shutdown = 0;

        // Create manager&worker threads
        pthread_create(&pool->managerID, NULL, manager, pool);
        for (int i = 0; i < min; ++i)
        {
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }

        return pool;
    } while(0);

    // Release resources
    if (pool&&pool->threadIDs) free(pool->threadIDs);
    if (pool&&pool->taskQ) free(pool->taskQ);
    if (pool) free(pool);

    return NULL;
}

void* worker(void* arg)
{
    ThreadPool *pool = (ThreadPool*)arg;

    // Worker threads continuously retrieve tasks from the task queue.
    while(1)
    {   // Threadpool's task queue currently is a shared resource, so thread synchronization is needed. 
        pthread_mutex_lock(&pool->mutexPool);
        // Check if the current task queue is empty.
        while(pool->queueSize == 0 && !pool->shutdown)
        {
            // Block worker thread
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
        }

        // 判断线程池是否关闭
        if (pool->shutdown)
        {
            // 线程池关闭的话，需要解锁 避免死锁
            pthread_mutex_unlock(&pool->mutexPool);

            // 打开锁之后, 让当前线程退出
            pthread_exit(NULL);
        }

        // 从任务队列中取出一个任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;

        // 移动头节点
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;

        pthread_mutex_unlock(&pool->mutexPool);

        // 忙线程需要加锁
        printf("thread %ld start working ...\n");
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);
        task.function(task.arg);    // (*task.function)(task.arg);
        free(task.arg);
        task.arg = NULL;

        // 任务执行完之后，busyNum 需要改回去
        printf("thread %ld end working ...\n");
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }

    return NULL;
}

void* manager(void* arg) 
{
    ThreadPool* pool = (ThreadPool*)arg;
    while(!pool->shutdown)
    {
        // 每隔3s检测一次
        sleep(3);

        // 取出线程池中task的数量以及线程的数量
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        // 取出忙的线程的数量
        pthread_mutex_lock(&pool->nutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->nutexBusy);
    }

    return NULL;
}