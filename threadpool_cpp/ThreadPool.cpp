#include "ThreadPool.hpp"
#include <iostream>
#include <string.h>
#include <string>
#include <unistd.h>
#include <cstdio>
using namespace std;

ThreadPool::ThreadPool(int min, int max)
{
    // 实例化任务队列
    taskQ = new TaskQueue;

    do 
    {
        threadIDs = new pthread_t[max];
        if (threadIDs == nullptr)
        {
            cout << "New threadIDs failed ..." << endl;
            break;
        }
        memset(threadIDs, 0, sizeof(pthread_t) * max);
        minNum = min;
        maxNum = max;
        busyNum = 0;
        liveNum = min;
        exitNum = 0;

        if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
            pthread_cond_init(&notFull, NULL) != 0)
        {
            cout << "mutex or condition init fail ... " << endl;
            break;
        }

        shutdown = false;

        // Create manager&worker threads
        pthread_create(&managerID, NULL, manager, this);
        for (int i = 0; i < min; ++i)
        {
            pthread_create(&threadIDs[i], NULL, worker, this);
        }

        return;
    } while(0);

    // Release resources
    if (threadIDs) delete[]threadIDs;
    if (taskQ) delete taskQ;
}

void ThreadPool::addTask(Task task)
{
    if (shutdown)
    {
        pthread_mutex_unlock(&mutexPool);
        return;
    }

    // 添加任务到队列的尾部
    taskQ->addTask(task);

    // 唤醒阻塞在条件变量的工作者线程
    pthread_cond_signal(&notEmpty);
}

int ThreadPool::getBusyNum()
{
    pthread_mutex_lock(&mutexPool);
    int busyNum = this->busyNum;
    pthread_mutex_unlock(&mutexPool);
    return busyNum;
}

int ThreadPool::getAliveNum()
{
    pthread_mutex_lock(&mutexPool);
    int aliveNum = this->liveNum;
    pthread_mutex_unlock(&mutexPool);
    return aliveNum;
}

void* ThreadPool::worker(void* arg)
{
    ThreadPool *pool = static_cast<ThreadPool*>(arg);

    // Worker threads continuously retrieve tasks from the task queue.
    while(true)
    {   // Threadpool's task queue currently is a shared resource, so thread synchronization is needed. 
        pthread_mutex_lock(&pool->mutexPool);
        // Check if the current task queue is empty.
        while(pool->taskQ->taskNumber() == 0 && !pool->shutdown)
        {
            // Block worker thread
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

            // 判断是不是要销毁线程（根据exitNum的多少）
            if (pool->exitNum > 0)
            {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum)
                {
                    pool->liveNum--;
                    // 注意 pool->mutexPool 锁在条件变量阻塞的时候就给到了线程
                    // 所以在线程退出之前需要解除避免死锁
                    pthread_mutex_unlock(&pool->mutexPool);
                    pool->threadExit();
                }
            }
        }

        // 判断线程池是否关闭
        if (pool->shutdown)
        {
            // 线程池关闭的话，需要解锁 避免死锁
            pthread_mutex_unlock(&pool->mutexPool);

            // 打开锁之后, 让当前线程退出
            pool->threadExit();
        }

        // 从任务队列中取出一个任务
        Task task = pool->taskQ->takeTask();

        pool->busyNum++;

        pthread_mutex_unlock(&pool->mutexPool);

        // 忙线程需要加锁
        cout<< "thread " << to_string(pthread_self()) <<" start working ...\n";
        task.function(task.arg);    // (*task.function)(task.arg);
        delete task.arg;
        task.arg = nullptr;

        // 任务执行完之后，busyNum 需要改回去
        cout<< "thread " << to_string(pthread_self()) <<" end working ...\n";
        pthread_mutex_lock(&pool->mutexPool);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexPool);
    }

    return NULL;
}

void* ThreadPool::manager(void* arg) 
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while(!pool->shutdown)
    {
        // 每隔3s检测一次
        sleep(3);

        // 取出线程池中task的数量以及线程的数量
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->taskQ->taskNumber();
        int liveNum = pool->liveNum;
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexPool);
    }

    // 添加线程
    // 任务的个数 > 存活的线程个数 && 存活的线程个数 < 最大线程数
    // 这种情况下线程忙不过来，需要添加一定量
    if (queueSize > liveNum && liveNum < pool->maxNum)
    {
        // 由于操作了 pool->liveNum，需要加锁（对线程池）
        pthread_mutex_lock(&pool->mutexPool);
        int counter = 0;
        for (int i = 0; i < pool->maxNum && counter < NUMBER
            && pool->liveNum < pool->maxNum; i++)
        {
            if (pool->threadIDs[i] == 0)
            {
                pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                counter++;
                pool->liveNum++;
            }
        }
        pthread_mutex_unlock(&pool->mutexPool);
    }

    // 销毁线程
    // 忙的线程 * 2 < 存活的线程数 && 存活的线程 > 最小线程数
    if (busyNum * 2 < liveNum && liveNum > pool->minNum)
    {
        // 还是销毁2个
        // exitNum 也是个共享变量，修改时候需要加锁
        pthread_mutex_lock(&pool->mutexPool);
        pool->exitNum = NUMBER;
        pthread_mutex_unlock(&pool->mutexPool);

        // 让工作的线程自行退出
        for (int i = 0; i < EXIT_THREAD_NUM; ++i)
        {
            // 唤醒阻塞在notEmpty条件变量上的工作线程
            pthread_cond_signal(&pool->notEmpty);
        }
    }

    return NULL;
}

void ThreadPool::threadExit()
{
    // 获取当前线程ID
    pthread_t tid = pthread_self();

    for (int i = 0; i < maxNum; ++i)
    {
        if (threadIDs[i] == tid)
        {
            // 线程退出时候，就要把线程id数组里面对于的值格式化
            threadIDs[i] = 0;
            cout << "threadExit() called, " << to_string(tid) << " exiting ...\n";
            break;
        }
    }
    pthread_exit(NULL);
}

ThreadPool::~ThreadPool()
{
    // 关闭线程池
    shutdown = true;

    // 阻塞回收管理者线程
    pthread_join(managerID, NULL);

    // 唤醒阻塞的消费者线程
    for (int i = 0; i < liveNum; ++i)
    {
        pthread_cond_signal(&notEmpty);
    }
    // 释放堆内存
    if (taskQ)
    {
        delete taskQ;
    }

    if (threadIDs)
    {
        delete[] threadIDs;
    }

    pthread_mutex_destory(&mutexPool);
    pthread_cond_destory(&notEmpty);
}