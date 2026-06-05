#pragma once
#include <queue>
#include <pthread.h>

using callback = void (*)(void* arg);

template <typename T>
// 任务结构体
struct Task<T>
{
    Task<T>(): function(nullptr), arg(nullptr) {}

    Task<T>(callback f, void* arg): function(f), arg(T*(arg)) {}

    callback function;
    T* arg;
};

template <typename T>
class TaskQueue
{
public:
    TaskQueue();
    ~TaskQueue();

    // 添加任务
    void addTask(Task<T> task);
    void addTask(callback f, void* arg);

    // 取出一个任务
    Task<T> takeTask();

    // 获取当前任务的个数
    inline int taskNumber()
    {
        return m_taskQ.size();
    }

private:
    pthread_mutex_t m_mutex;
    std::queue<Task<T>> m_taskQ;
};