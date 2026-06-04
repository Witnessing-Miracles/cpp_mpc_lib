#ifndef _THREADPOOL_
#define _THREADPOOL_

typedef struct ThreadPool ThreadPool;

/**
 * Create threadpool
 * Parameters: maximun and minimum number of worker threads, task queue's size
 */
ThreadPool *threadPoolCreate(int min, int max, int queueSize);

// Close threadpool
int threadPoolDestroy(ThreadPool* pool);

// Add tasks into threadpool
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);

// Acquire the number of working threads in pool
int threadPoolBusyNum(ThreadPool* pool);

// Acquire the number of alive threads in pool
int threadPoolAliveNum(ThreadPool* pool);

// Worker thread task function
void* worker(void* arg);
void* manager(void* arg);
void threadExit(ThreadPool* pool);

#endif // !_THREADPOOL_