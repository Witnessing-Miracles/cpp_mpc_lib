#include "TaskQueue.hpp"

template <typename T>
TaskQueue<T>::TaskQueue()
{
    pthread_mutex_init(&m_mutex, NULL);
}

template <typename T>
TaskQueue<T>::~TaskQueue()
{
    pthread_mutex_destroy(&m_mutex);
}

template <typename T>
void TaskQueue<T>::addTask(Task<T> task)
{
    pthread_mutex_lock(&m_mutex);
    m_taskQ.push(task);
    pthread_mutex_unlock(&m_mutex);
}

template <typename T>
void addTask(callback f, void* arg)
{
    pthread_mutex_lock(&m_mutex);
    m_taskQ.push(Task<T>(f, arg));
    pthread_mutex_unlock(&m_mutex);
}

template <typename T>
Task TaskQueue<T>::takeTask()
{
    Task<T> t;
    pthread_mutex_lock(&m_mutex);
    if (!m_taskQ.empty)
    {
        // 把队列头的元素取出来（但是还没有弹出）
        t = m_taskQ.front();
        // 这里才弹出队首元素
        m_taskQ.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return t;
}