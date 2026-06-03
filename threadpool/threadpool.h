#ifndef _THREADPOOL_
#define _THREADPOOL_

typedef struct ThreadPool ThreadPool;

/**
 * Create threadpool
 * Parameters: maximun and minimum number of worker threads, task queue's size
 */
ThreadPool *threadPoolCreate(int min, int max, int queueSize);

// Close threadpool

// Add tasks into threadpool

// Acquire the number of working threads in pool

// Acquire the number of alive threads in pool

// Worker thread task function
void* worker(void* arg);
void* manager(void* arg);

#endif // !_THREADPOOL_