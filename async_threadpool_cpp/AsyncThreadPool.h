#pragma once
#include <thread>
#include <vector>
#include <atomic>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <map>
#include <future>
#include <memory>

using namespace std;

class AsyncThreadPool
{
public:
    AsyncThreadPool(int min = 3, int max = thread::hardware_concurrency());
    ~AsyncThreadPool();

    template<typename F, typename... Args>
    auto addTask(F&& func, Args&&... args) -> future<invoke_result_t<F, Args...>>
    {
        using ReturnType = invoke_result_t<F, Args...>;

        auto task = make_shared<packaged_task<ReturnType()>>(
            bind(forward<F>(func), forward<Args>(args)...)
        );

        future<ReturnType> result = task->get_future();

        {
            lock_guard<mutex> locker(m_queueMutex);
            m_tasks.emplace([task](){ (*task)(); });
        }
        m_condition.notify_one();

        return result;
    }

private:
    void manager(void);
    void worker(void);

private:
    thread* m_manager;
    map<thread::id, thread> m_workers;
    vector<thread::id> m_ids;
    atomic<int> m_minThread;
    atomic<int> m_maxThread;
    atomic<int> m_curThread;
    atomic<int> m_idleThread;
    atomic<int> m_exitThread;
    atomic<bool> m_stop;

    queue<function<void(void)>> m_tasks;
    mutex m_queueMutex;
    mutex m_idsMutex;
    condition_variable m_condition;
};