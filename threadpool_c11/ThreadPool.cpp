#include "ThreadPool.h"

ThreadPool::ThreadPool(int min, int max) : m_minThread(min), m_maxThread(max),
m_stop(false), m_curThread(min), m_idleThread(min)
{
    /**
     * 创建管理者线程
     * 参数1: 线程处理函数
     * 参数2: 线程处理函数的所有者, 当前类的实例 也就是this
     */
    m_manager = new thread(&ThreadPool::manager, this);   // 为什么加 &, 固定语法不加的话编译器报错

    /**
     * 创建工作者线程
     * 
     *
     */
    for (int i = 0; i < min; ++i)
    {
        // thread t(&ThreadPool::worker, this);
        // m_workers.push_back(t); 这里会触发拷贝
        // 注意即便用了emplace_back, 传上面的临时对象还是会触发拷贝
        // 参数这里传一个匿名对象, 当该对象从容器里面弹出的时候就被销毁了
        m_workers.emplace_back(thread(&ThreadPool::worker, this));
    }
}

ThreadPool::~ThreadPool()
{}

/**
 * 任务队列是一块共享访问区域, 需要对线程访问进行同步
 * 这里使用 C++ 中更高级的类, 更加安全地管理线程同步避免死锁
 * 管理互斥锁的模板类有两个: lock_guard 和 unique_lock
 */
void ThreadPool::addTask(function<void(void)> task)
{
    {
        // lock 对象创建完成后会对互斥锁对象m_queueMutex加锁(m_queueMutex 必须是打开的不然调用这行就阻塞了)
        lock_guard<mutex> locker(m_queueMutex);
        // 这里必须使用 move 来避免拷贝
        m_tasks.emplace(std::move(task));
    }  // 出作用域的时候 locker 对象析构时候解锁 m_queueMutex

    // 使用上述 {} 的原因是只需要锁住往队列里面添加 task 的操作

    // 使用条件变量通知一个工作者线程
    m_condition.notify_one();
}

void ThreadPool::manager(void)
{}
