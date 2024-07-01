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
using std::thread;
using std::unique_lock;
using std::unique_ptr;
using std::vector;

// 线程池模式枚举类
enum class PoolMode
{
    MODE_FIXED,  // 固定数量的线程池模式
    MODE_CACHED, // 动态增长的线程池模式
};

// Any类，可以接受任意类型数据
class Any
{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any &) = delete;
    Any &operator=(const Any &) = delete;
    Any(Any &&any) = default;
    Any &operator=(Any &&any) = default;

    // 一个可以接受任意类型数据的构造函数
    template <typename T>
    Any(T data) : base_(make_unique<Derived<T>>(data))
    {
    }

    // 从Any类对象中提取原类型对象
    template <typename T>
    T Cast()
    {
        Derived<T> *p = dynamic_cast<Derived<T> *>(base_.get());
        if (p == nullptr)
        {
            throw "bad cast!";
        }
        return p->data_;
    }

private:
    // 基类类型
    class Base
    {
    public:
        virtual ~Base() = default;
    };
    // 派生类类型（模板）
    template <typename T>
    class Derived : public Base
    {
    public:
        Derived(T data) : data_(data) {}
        T data_; // 保存了任意类型数据
    };

    unique_ptr<Base> base_; // 基类类型的智能指针
};

// 实现一个semaphore类
class Semaphore
{
public:
    Semaphore(int limit = 0)
        : resLimit_(limit), mtx_(make_unique<mutex>()), cond_(make_unique<condition_variable>()), isExit_(make_unique<std::atomic_bool>(false))
    {
    }
    ~Semaphore()
    {
        isExit_->store(true);
    }
    Semaphore(Semaphore &&) = default;
    Semaphore &operator=(Semaphore &&) = default;

    // 获取信号量
    void wait()
    {
        if (isExit_->load())
            return;
        unique_lock<mutex> lock(*mtx_);
        cond_->wait(lock, [&]() -> bool
                    { return resLimit_ > 0; });
        resLimit_--;
    }

    // 增加信号量资源计数
    void post()
    {
        if (isExit_->load())
            return;
        unique_lock<mutex> lock(*mtx_);
        resLimit_++;
        cond_->notify_all();
    }

private:
    int resLimit_;                        // 资源计数
    unique_ptr<mutex> mtx_;               // 互斥锁
    unique_ptr<condition_variable> cond_; // 条件变量
    unique_ptr<std::atomic_bool> isExit_; // 是否析构了
};

class Task; // 前置声明

// Result类，用于获取task执行结果
class Result
{
public:
    Result(shared_ptr<Task> sp = nullptr, bool isValid = true);
    ~Result()
    {
        isExit_->store(true);
    }
    Result(Result &&);
    Result &operator=(Result &&r);

    // 设置任务执行结果
    void setVal(Any any);

    // 获取任务执行结果
    Any get();

private:
    Any any_;
    Semaphore sem_;
    shared_ptr<Task> task_;
    bool isValid_;
    unique_ptr<std::atomic_bool> isExit_; // 是否析构了
};

// task抽象基类
// 用户可以定义一个继承自task的任务类，实现run函数来完成目标任务
class Task
{
public:
    Task();
    ~Task() = default;
    // 任务类需要定义的接口，实现真正的用户任务
    virtual Any run() = 0;

    // 线程函数中调用的接口
    void exec();

    // 设置Result*
    void setResult(Result *res);

private:
    Result *result_;
};

// 线程类
class Thread
{
public:
    using ThreadFunc = function<void(int)>;

    Thread(ThreadFunc threadfunc);
    // 启动线程函数
    void start();

    // 获取线程id
    int getId() const;

private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_; // 保存线程id
    Result result_;
};

// 线程池类
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    //  启动线程池函数
    void start(int initThreadSize = std::thread::hardware_concurrency());

    // 设置task数量上限
    void setTaskMaxThreshold(int taskMaxThreshold);

    // 设置线程池模式
    void setMode(PoolMode poolMode);

    // 设置最大线程数量
    void setMaxThreadSize(int maxThreadSize);

    // 向task队列中提交任务
    Result submitTask(shared_ptr<Task> sp);

    // 禁止拷贝构造和拷贝赋值
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // 线程入口函数
    void threadFunc(int threadId);

    bool checkPoolState();

private:
    vector<unique_ptr<Thread>> threads_; // 线程对列
    queue<shared_ptr<Task>> taskQue_;    // 任务队列
    int initThreadSize_;                 // 初始线程数量
    atomic_int curThreadSize_;           // 当前线程数量
    atomic_int maxThreadSize_;           // 最大线程数量
    atomic_int idleThreadSize_;          // 空闲线程的数量

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
