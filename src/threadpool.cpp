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
        threads_.emplace_back(new Thread(bind(&ThreadPool::threadFunc, this)));
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
}

// 线程入口函数
void ThreadPool::threadFunc()
{
    cout << "start thread, tid: " << std::this_thread::get_id() << endl;
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
