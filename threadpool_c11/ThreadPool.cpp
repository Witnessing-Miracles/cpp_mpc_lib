#include "ThreadPool.h"
#include <iostream>

ThreadPool::ThreadPool(int min, int max) : m_minThread(min), m_maxThread(max),
m_stop(false), m_curThread(min), m_idleThread(min), m_exitThread(0)   // FIX: initialise m_exitThread
{
    /**
     * 创建管理者线程
     * 参数1: 线程处理函数
     * 参数2: 线程处理函数的所有者, 当前类的实例 也就是this
     */
    m_manager = new thread(&ThreadPool::manager, this);   // 为什么加 &, 固定语法不加的话编译器报错

    /**
     * 创建工作者线程
     */
    for (int i = 0; i < min; ++i)
    {
        thread t(&ThreadPool::worker, this);
        m_workers.insert(make_pair(t.get_id(), move(t)));
    }
}

// FIX: destructor now shuts down all threads cleanly
ThreadPool::~ThreadPool()
{
    m_stop.store(true);
    m_condition.notify_all();

    if (m_manager && m_manager->joinable())
    {
        m_manager->join();
    }
    delete m_manager;

    for (auto& pair : m_workers)    // 记住, 线程对象不允许拷贝. 这里必须用引用
    {
        if (pair.second.joinable()) // joinable 用于判断当前线程是否可连接
            cout << "------ 线程 " << pair.second.get_id() << " 将要退出了 ......" << endl;
            pair.second.join();
    }
}

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
{
    while (!m_stop.load())
    {
        // 每3s检查一次线程池里面的线程数和空闲线程数之间的关系
        this_thread::sleep_for(chrono::seconds(1));
        int idel = m_idleThread.load();
        int cur = m_curThread.load();

        // 满足下述条件就认为当前空闲线程过多
        if (idel > cur/2 && cur > m_minThread)
        {
            // 每次销毁两个线程
            m_exitThread.store(2);      // 原子变量的标准赋值方法, 等同于 m_exitThread = 2 
            m_condition.notify_all();   // 唤醒所有阻塞在条件变量上的线程

            // FIX: collect IDs to join, then release m_idsMutex before calling join()
            // to avoid deadlock (worker needs m_idsMutex to push its own ID on exit)
            vector<thread::id> toJoin;
            {
                lock_guard<mutex> lck(m_idsMutex);
                toJoin = m_ids;
                m_ids.clear();
            }

            for (auto id : toJoin)
            {
                auto it = m_workers.find(id);
                if (it != m_workers.end())
                {
                    // FIX: save the ID before erase(); accessing (*it) after erase is UB
                    thread::id tid = it->first;
                    cout << "------ 被销毁的线程的ID是: " << tid << endl;
                    it->second.join();    // 阻塞当前 manager 对应的线程 等待线程对象里面的任务执行完毕
                    m_workers.erase(it);    // 把线程对象从map移除 它就会被自动销毁
                }
            }
        }
        else if (idel == 0 && cur < m_maxThread)
        {
            thread t(&ThreadPool::worker, this);
            m_workers.insert(make_pair(t.get_id(), move(t)));
            m_curThread++;
            m_idleThread++;
        }
    }
}

void ThreadPool::worker(void)
{
    while (!m_stop.load())   // 调用原子变量的 load 方法 (这是线程安全的)
    {
        function<void(void)> task = nullptr;
        {
            unique_lock<mutex> locker(m_queueMutex);
            while (m_tasks.empty() && !m_stop)
            {
                // 调用 wait 时候, locker 对象管理的互斥锁会被自动解锁以避免死锁
                // 生产者线程生产一个 task 以后就会唤醒一个阻塞的线程(这里讨论 notify_one), 
                // 唤醒的线程拿到任务时候会重新通过 locker 对象把 m_queueMutex 加上锁 
                m_condition.wait(locker);
                if (m_exitThread.load() > 0)
                {
                    m_curThread--;
                    m_idleThread--;
                    m_exitThread--;
                    cout << "------ 退出的线程ID为: " << this_thread::get_id() << endl;
                    lock_guard<mutex> lck(m_idsMutex);
                    m_ids.emplace_back(this_thread::get_id());
                    return;
                }
            }
            // 队列中取任务
            if (!m_tasks.empty())
            {
                /**
                 * C++ 极其注重异常安全(Exception Safety)
                 * 如果一个函数既负责把数据移出来，又负责在底层销毁节点，万一在拷贝/移动数据的过程中由于内存不足
                 * 抛出了异常，此时数据没成功拿到，而底层的节点却已经被删了，这个数据就彻底凭空消失(丢失)了
                 * 因此，C++ 规定:
                 * 先用 front() 安全地把数据拿走 (即使报错，原数据还在队列里安然无恙)
                 * 确认拿走安全后，再用 pop() 物理切断、销毁底层节点
                 * move 只是搬空了箱子，pop 才是把箱子扔掉。两个必须组合使用，缺一不可！
                 */
                cout << "取出了一个任务 ... " << endl;
                task = move(m_tasks.front());     // 不使用 move 的话这段代码执行之后会发生一个拷贝
                m_tasks.pop();      // 弹出已经被取出的任务
            }  // unique_lock 的存在只需要维持到每一次任务从队列取出
        }
        if (task)
        {
            m_idleThread--;     // 原子变量, 执行的时候线程安全
            task();
            m_idleThread++;
        }
    }
}

void calc(int x, int y)
{
    int z = x + y;
    cout << "z = " << z << endl;
    this_thread::sleep_for(chrono::seconds(2));
}

int main()
{
    ThreadPool pool;
    for (int i = 0; i < 10; ++i)
    {
        /**
         * 现在提供的 calc 是带参的, 直接传递给 addTask 会有问题
         * 由于少参数, 需要事先进行绑定. 可调用对象绑定器叫做 bind
         */
        
        // bind 第一个参数是函数地址, 后面的参数是函数实参, 这里有两个, 分别给 i 和 i*2
        auto obj = bind(calc, i, i*2);
        
        pool.addTask(obj);
    }

    // 阻塞主线程
    this_thread::sleep_for(chrono::seconds(15));

    return 0;
}