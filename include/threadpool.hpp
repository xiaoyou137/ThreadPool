#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

using std::atomic_int;
using std::condition_variable;
using std::mutex;
using std::queue;
using std::shared_ptr;
using std::thread;
using std::vector;

// 线程池模式枚举类
enum class PoolMode
{
    MODE_FIXED,  // 固定数量的线程池模式
    MODE_CACHED, // 动态增长的线程池模式
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
private:
};

// 线程池类
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    //  启动线程池函数
    void start();

    // 设置线程池模式
    void setMode(PoolMode poolMode);

private:
    vector<thread *> threads_;        // 线程对列
    queue<shared_ptr<Task>> taskQue_; // 任务队列
    atomic_int taskCount_;            // 任务队列中任务的个数
    int taskMaxThreshold_;            // 任务队列中任务最大数量

    condition_variable cvNotFull_;  // 任务队列未满，可以向其中提交任务
    condition_variable cvNotEmpty_; // 任务队列不空，工作线程可以从其中取出任务执行
    mutex taskQueMtx_;              // 保证任务队列线程安全的互斥锁

    PoolMode poolMode_; // 线程池的工作模式：fixed 和 cached
};

#endif
