#include "threadpool.hpp"

/* ==============================================================*/
// 线程池ThreadPool类方法实现
/*===============================================================*/

ThreadPool::ThreadPool()
    : initThreadSize_(4), taskCount_(0), taskMaxThreshold_(1024), poolMode_(PoolMode::MODE_FIXED)
{
}

ThreadPool::~ThreadPool()
{
}

//  启动线程池函数
void ThreadPool::start(int initThreadSize)
{
    initThreadSize_ = initThreadSize;

    // 创建线程对象
    for (int i = 0; i < initThreadSize_; i++)
    {
        auto t = std::make_unique<Thread>(bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(t));
    }

    // 启动线程函数
    for (int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start();
    }
}

// 设置线程池模式
void ThreadPool::setMode(PoolMode poolMode)
{
    poolMode_ = poolMode;
}

// 设置task数量上限
void ThreadPool::setTaskMaxThreshold(int taskMaxThreshold)
{
    taskMaxThreshold_ = taskMaxThreshold;
}

// 向task队列中提交任务
void ThreadPool::submitTask(shared_ptr<Task> sp)
{
    // 获取锁
    unique_lock<mutex> lock(taskQueMtx_);

    // 判断task队列是否满了，如果满了进入等待
    // 用户等待超过1s钟，即认为提交任务失败，返回
    if (!cvNotFull_.wait_for(lock, std::chrono::seconds(1),
                             [&]()
                             { return taskQue_.size() < taskMaxThreshold_; }))
    {
        cout << "task queue is full, submit failed!" << endl;
        return;
    }

    // 向task队列中添加任务
    taskQue_.push(sp);
    taskCount_++;

    // 通知工作线程
    cvNotEmpty_.notify_all();
}

// 线程入口函数
void ThreadPool::threadFunc()
{
    for (;;)
    {
        shared_ptr<Task> task;
        {
            // 获取锁
            unique_lock<mutex> lock(taskQueMtx_);
            // 等待cvNotEmpty_通知
            cvNotEmpty_.wait(lock, [&]() { return !taskQue_.empty(); });
            // 从task队列中取出一个任务
            task = taskQue_.front();
            taskQue_.pop();
            taskCount_--;
            // 如果队列中还有任务，通知其他线程
            if(!taskQue_.empty())
            {
                cvNotEmpty_.notify_all();
            }
            // 通知cvNotFull_
            cvNotFull_.notify_all();
        } // 释放锁

        // 执行任务
        task->run();
    }
}

/* ==============================================================*/
// 线程Thread类方法实现
/*===============================================================*/

Thread::Thread(ThreadFunc threadfunc)
    : func_(threadfunc)
{
}

// 启动线程函数
void Thread::start()
{
    thread t(func_);
    t.detach();
}
