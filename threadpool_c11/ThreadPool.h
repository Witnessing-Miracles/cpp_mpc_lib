#pragma once
#include <thread>
#include <vector>
#include <atomic>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

using namespace std;
/**
 * 构成:
 * 1. 管理者线程 -> 子线程, 1个
 * - 控制工作线程的数量: 增加或减少
 * 
 * 2. 工作者线程 -> 子线程, 多个
 * - 从任务队列中取任务并处理
 * - 任务队列为空, 就阻塞工作者线程(通过条件变量)
 * - 因为任务队列是共享资源, 工作者线程访问它的时候需要同步(用到互斥锁)
 * - 需要知道运行的工作者线程的数量 以及空闲的工作者线程数量 以供管理者线程制定策略
 * - 指定最大和最小工作者线程的数量
 *
 * 3. 任务队列 -> STL 里面的 queue
 * - 需要用到互斥锁
 * - 需要用到条件变量
 *
 * 还可以给线程池一个开关(bool类型的变量), 当开关关闭时候就销毁线程池里面的所有线程, 锁, 条件变量 以及分配的内存等资源
 */
 
class ThreadPool
{
public:
    /**
     * 参数: min 表示线程池中的最小线程数量, max 表示最大线程数量
     * 函数参数的默认值从右向左给予 (本机CPU4核, 通过Intel超线程技术, 让每个物理核心能高效处理两个任务
     * 从而在系统中显示出8个逻辑处理器即最大线程支持数为8)
     */
    // thread 类中的 hardware_concurrency() 函数能计算出本机的最先线程支持数
    ThreadPool(int min = 2, int max = 8 /* thread::hardware_concurrency() */);
    ~ThreadPool();

    /**
     * 添加任务到任务队列
     * 任务队列对应的也是容器, 里面存储的是可调用对象
     */
    void addTask(function<void(void)> task);

private:
    void manager(void);
    void worker(void);

private:
    thread* m_manager;
    vector<thread> m_workers;
    atomic<int> m_minThread;
    atomic<int> m_maxThread;
    atomic<int> m_curThread;
    atomic<int> m_idleThread;
    atomic<bool> m_stop;
    
    // queue 里装的是可调用函数对象
    queue<function<void(void)>> m_tasks;
    mutex m_queueMutex;
    condition_variable m_condition;
};