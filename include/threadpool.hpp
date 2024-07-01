#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <functional>
#include <iostream>
#include <future>
#include <algorithm>

using std::atomic_int;
using std::bind;
using std::condition_variable;
using std::cout;
using std::endl;
using std::function;
using std::make_unique;
using std::mutex;
using std::queue;
using std::shared_ptr;
using std::make_shared;
using std::thread;
using std::unique_lock;
using std::unique_ptr;
using std::vector;
using std::packaged_task;

const int MAX_TASK_SIZE = 1024;
const int MAX_THREAD_SIZE = 10;
const int MAX_THREAD_IDLE_TIME = 10;

// 线程池模式枚举类
enum class PoolMode
{
    MODE_FIXED,  // 固定数量的线程池模式
    MODE_CACHED, // 动态增长的线程池模式
};

// 线程类
class Thread
{
public:
    using ThreadFunc = function<void(int)>;

    Thread(ThreadFunc threadfunc)
        : func_(threadfunc), threadId_(generateId_++)
    {
    }
    // 启动线程函数
    void start()
    {
        thread t(func_, threadId_);
        t.detach();
    }

    // 获取线程id
    int getId() const
    {
        return threadId_;
    }

private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_; // 保存线程id
};

int Thread::generateId_ = 0;

// 线程池类
class ThreadPool
{
public:
    ThreadPool()
        : initThreadSize_(4), curThreadSize_(0), maxThreadSize_(MAX_THREAD_SIZE), idleThreadSize_(0), taskCount_(0), taskMaxThreshold_(MAX_TASK_SIZE), poolMode_(PoolMode::MODE_FIXED), isPoolRunning(false)
    {
    }

    ~ThreadPool()
    {
        isPoolRunning = false;

        unique_lock<mutex> lock(taskQueMtx_);
        cvNotEmpty_.notify_all();
        cvExit_.wait(lock, [&]() -> bool
                     { return threads_.empty(); });
    }

    //  启动线程池函数
    void start(int initThreadSize = std::thread::hardware_concurrency())
    {
        initThreadSize_ = initThreadSize;
        isPoolRunning = true;

        // 创建线程对象
        for (int i = 0; i < initThreadSize_; i++)
        {
            auto t = std::make_unique<Thread>(bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            threads_.emplace_back(std::move(t));
            curThreadSize_++;
            idleThreadSize_++;
        }

        // 启动线程函数
        for (int i = 0; i < initThreadSize_; i++)
        {
            threads_[i]->start();
        }
    }

    // 设置task数量上限
    void setTaskMaxThreshold(int taskMaxThreshold)
    {
        if (checkPoolState())
            return;
        taskMaxThreshold_ = taskMaxThreshold;
    }

    // 设置线程池模式
    void setMode(PoolMode poolMode)
    {
        if (checkPoolState())
            return;
        poolMode_ = poolMode;
    }

    // 设置最大线程数量
    void setMaxThreadSize(int maxThreadSize)

    {
        if (checkPoolState())
            return;
        maxThreadSize_ = maxThreadSize;
    }

    // 向task队列中提交任务,使用引用折叠+完美转发+可变参数模板实现
    template <typename Func, typename... Args>
    auto submitTask(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
    {
        using Rtype = decltype(func(args...));
        auto task = make_shared<packaged_task<Rtype()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<Rtype> result = task->get_future();

        // 获取锁
        unique_lock<mutex> lock(taskQueMtx_);

        // 判断task队列是否满了，如果满了进入等待
        // 用户等待超过1s钟，即认为提交任务失败，返回
        if (!cvNotFull_.wait_for(lock, std::chrono::seconds(1),
                                 [&]()
                                 { return taskQue_.size() < taskMaxThreshold_; }))
        {
            cout << "task queue is full, submit failed!" << endl;
            auto task = make_shared<packaged_task<Rtype()>>(
                []() -> Rtype
                { return Rtype(); });
            (*task)();
            auto result = task->get_future();
            return result;
        }

        // 向task队列中添加任务
        taskQue_.push(make_shared<Task>(
            [task]()
            { (*task)(); }));
        taskCount_++;

        // 通知工作线程
        cvNotEmpty_.notify_all();

        // cached模式下，动态增加线程数量
        if (poolMode_ == PoolMode::MODE_CACHED && taskCount_ > idleThreadSize_ && curThreadSize_ < maxThreadSize_)
        {
            threads_.emplace_back(std::move(make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1))));
            threads_.back()->start();
            curThreadSize_++;
            idleThreadSize_++;
            cout << "creat new thread! " << endl;
        }

        return result;
    }

    // 禁止拷贝构造和拷贝赋值
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // 线程入口函数
    void threadFunc(int threadId)
    {
        int id = threadId;
        // 记录线程启动时间
        auto lastTime = std::chrono::high_resolution_clock().now();

        // 线程池析构时，等待所有任务执行完成再析构。
        for (;;)
        {
            shared_ptr<Task> task;
            {
                // 获取锁
                unique_lock<mutex> lock(taskQueMtx_);
                // 锁+双重检查
                while (taskQue_.empty())
                {
                    if (!isPoolRunning)
                    {
                        // 释放Thread资源
                        auto it = std::find_if(threads_.begin(), threads_.end(), [&](auto &up)
                                               { return up->getId() == id; });
                        if (it != threads_.end())
                        {
                            threads_.erase(it);
                            cout << "threadid: " << std::this_thread::get_id() << " exit!" << endl;
                        }
                        cvExit_.notify_all();
                        return;
                    }
                    if (poolMode_ == PoolMode::MODE_CACHED)
                    {
                        if (std::cv_status::timeout == cvNotEmpty_.wait_for(lock, std::chrono::seconds(1)))
                        {
                            auto now = std::chrono::high_resolution_clock().now();
                            auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                            if (dur.count() > MAX_THREAD_IDLE_TIME && curThreadSize_ > initThreadSize_)
                            {
                                // 从thread_中删除当前线程
                                auto it = std::find_if(threads_.begin(), threads_.end(), [&](auto &up)
                                                       { return up->getId() == id; });
                                if (it != threads_.end())
                                {
                                    threads_.erase(it);
                                    cout << "threadid: " << std::this_thread::get_id() << " exit!" << endl;
                                    // 更新记录数据
                                    curThreadSize_--;
                                    idleThreadSize_--;
                                }
                                return;
                            }
                        }
                    }
                    else
                    {
                        cout << "threadid: " << std::this_thread::get_id() << " 尝试获取任务!" << endl;
                        cvNotEmpty_.wait(lock);
                    }
                }

                // 从task队列中取出一个任务
                task = taskQue_.front();
                taskQue_.pop();
                taskCount_--;
                idleThreadSize_--;
                // 如果队列中还有任务，通知其他线程
                if (!taskQue_.empty())
                {
                    cvNotEmpty_.notify_all();
                }
                // 通知cvNotFull_
                cvNotFull_.notify_all();
            } // 释放锁

            // 执行任务
            if (task != nullptr)
            {
                (*task)();
            }
            idleThreadSize_++;
            // 记录线程开始空闲的时间
            lastTime = std::chrono::high_resolution_clock().now();
        }
    }

    bool checkPoolState()
    {
        return isPoolRunning;
    }

private:
    vector<unique_ptr<Thread>> threads_; // 线程对列

    using Task = function<void()>;
    queue<shared_ptr<Task>> taskQue_; // 任务队列
    int initThreadSize_;              // 初始线程数量
    atomic_int curThreadSize_;        // 当前线程数量
    atomic_int maxThreadSize_;        // 最大线程数量
    atomic_int idleThreadSize_;       // 空闲线程的数量

    atomic_int taskCount_; // 任务队列中任务的个数
    int taskMaxThreshold_; // 任务队列中任务最大数量

    condition_variable cvNotFull_;  // 任务队列未满，可以向其中提交任务
    condition_variable cvNotEmpty_; // 任务队列不空，工作线程可以从其中取出任务执行
    condition_variable cvExit_;     // 清理回收线程池
    mutex taskQueMtx_;              // 保证任务队列线程安全的互斥锁

    PoolMode poolMode_;       // 线程池的工作模式：fixed 和 cached
    atomic_int isPoolRunning; // 当前线程池是否启动
};

#endif
