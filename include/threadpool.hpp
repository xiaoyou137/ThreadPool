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
using std::condition_variable;
using std::mutex;
using std::queue;
using std::shared_ptr;
using std::unique_ptr;
using std::make_unique;
using std::unique_lock;
using std::thread;
using std::vector;
using std::function;
using std::bind;
using std::cout;
using std::endl;

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
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;


    // 一个可以接受任意类型数据的构造函数
    template<typename T>
    Any(T data) : base_(make_unique<Derived<T>>(data)) {}

    // 从Any类对象中提取原类型对象
    template<typename T>
    T Cast()
    {
        Derived<T>* p = dynamic_cast<Derived<T>*>(base_.get());
        if(p == nullptr)
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
    template<typename T>
    class Derived : public Base
    {
    public:
        Derived(T data) : data_(data) {}
        T data_; // 保存了任意类型数据
    };

    unique_ptr<Base> base_; // 基类类型的智能指针
};

// task抽象基类
// 用户可以定义一个继承自task的任务类，实现run函数来完成目标任务
class Task
{
public:
    virtual void run() = 0;
};

// 线程类
class Thread
{
public:
    using ThreadFunc = function<void()>;

    Thread(ThreadFunc threadfunc);
    // 启动线程函数
    void start();
private:

    ThreadFunc func_;
};

// 线程池类
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    //  启动线程池函数
    void start(int initThreadSize = 4);

    // 设置task数量上限
    void setTaskMaxThreshold(int taskMaxThreshold);

    // 设置线程池模式
    void setMode(PoolMode poolMode);

    // 向task队列中提交任务
    void submitTask(shared_ptr<Task> sp);

    // 禁止拷贝构造和拷贝赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
private:
    // 线程入口函数
    void threadFunc();

private:
    vector<unique_ptr<Thread>> threads_;        // 线程对列
    queue<shared_ptr<Task>> taskQue_; // 任务队列
    int initThreadSize_;              // 初始线程数量
    atomic_int taskCount_;            // 任务队列中任务的个数
    int taskMaxThreshold_;            // 任务队列中任务最大数量

    condition_variable cvNotFull_;  // 任务队列未满，可以向其中提交任务
    condition_variable cvNotEmpty_; // 任务队列不空，工作线程可以从其中取出任务执行
    mutex taskQueMtx_;              // 保证任务队列线程安全的互斥锁

    PoolMode poolMode_; // 线程池的工作模式：fixed 和 cached
};

#endif
