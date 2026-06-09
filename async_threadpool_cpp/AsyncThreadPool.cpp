#include "AsyncThreadPool.h"
#include <iostream>

AsyncThreadPool::AsyncThreadPool(int min, int max)
    : m_minThread(min), m_maxThread(max),
      m_curThread(min), m_idleThread(min),
      m_exitThread(0), m_stop(false)
{
    m_manager = new thread(&AsyncThreadPool::manager, this);

    for (int i = 0; i < min; ++i)
    {
        thread t(&AsyncThreadPool::worker, this);
        m_workers.insert(make_pair(t.get_id(), move(t)));
    }
}

AsyncThreadPool::~AsyncThreadPool()
{
    m_stop.store(true);
    m_condition.notify_all();

    if (m_manager && m_manager->joinable())
        m_manager->join();
    delete m_manager;

    for (auto& pair : m_workers)
    {
        if (pair.second.joinable())
            pair.second.join();
    }
}

void AsyncThreadPool::manager(void)
{
    while (!m_stop.load())
    {
        this_thread::sleep_for(chrono::seconds(1));
        int idel = m_idleThread.load();
        int cur  = m_curThread.load();

        if (idel > cur / 2 && cur > m_minThread)
        {
            m_exitThread.store(2);
            m_condition.notify_all();

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
                    thread::id tid = it->first;
                    cout << "------ 被销毁的线程ID: " << tid << endl;
                    it->second.join();
                    m_workers.erase(it);
                }
            }
        }
        else if (idel == 0 && cur < m_maxThread)
        {
            thread t(&AsyncThreadPool::worker, this);
            m_workers.insert(make_pair(t.get_id(), move(t)));
            m_curThread++;
            m_idleThread++;
        }
    }
}

void AsyncThreadPool::worker(void)
{
    while (!m_stop.load())
    {
        function<void(void)> task = nullptr;
        {
            unique_lock<mutex> locker(m_queueMutex);
            while (m_tasks.empty() && !m_stop)
            {
                m_condition.wait(locker);
                if (m_exitThread.load() > 0)
                {
                    m_curThread--;
                    m_idleThread--;
                    m_exitThread--;
                    cout << "------ 退出的线程ID: " << this_thread::get_id() << endl;
                    lock_guard<mutex> lck(m_idsMutex);
                    m_ids.emplace_back(this_thread::get_id());
                    return;
                }
            }
            if (!m_tasks.empty())
            {
                task = move(m_tasks.front());
                m_tasks.pop();
            }
        }
        if (task)
        {
            m_idleThread--;
            task();
            m_idleThread++;
        }
    }
}

// -------- 测试 --------
int calc(int x, int y)
{
    return x + y;
}

int main()
{
    AsyncThreadPool pool;

    vector<future<int>> results;

    for (int i = 0; i < 10; ++i)
    {
        auto fut = pool.addTask(calc, i, i * 2);
        results.push_back(move(fut));
    }

    for (auto& fut : results)
    {
        cout << "result = " << fut.get() << endl;
    }

    return 0;
}