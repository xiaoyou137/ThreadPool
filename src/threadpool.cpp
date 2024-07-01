#include "threadpool.hpp"
#include <algorithm>

/* ==============================================================*/
// 线程池ThreadPool类方法实现
/*===============================================================*/

const int MAX_TASK_SIZE = 1024;
const int MAX_THREAD_SIZE = 10;
const int MAX_THREAD_IDLE_TIME = 10;

ThreadPool::ThreadPool()
    : initThreadSize_(4), curThreadSize_(0), maxThreadSize_(MAX_THREAD_SIZE), idleThreadSize_(0), taskCount_(0), taskMaxThreshold_(MAX_TASK_SIZE), poolMode_(PoolMode::MODE_FIXED), isPoolRunning(false)
{
}

// 线程池析构，回收线程资源
ThreadPool::~ThreadPool()
{
    isPoolRunning = false;

    unique_lock<mutex> lock(taskQueMtx_);
    cvNotEmpty_.notify_all();
    cvExit_.wait(lock, [&]() -> bool
                 { return threads_.empty(); });
}

//  启动线程池函数
void ThreadPool::start(int initThreadSize)
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

// 设置线程池模式
void ThreadPool::setMode(PoolMode poolMode)
{
    if (checkPoolState())
        return;
    poolMode_ = poolMode;
}

// 设置task数量上限
void ThreadPool::setTaskMaxThreshold(int taskMaxThreshold)
{
    if (checkPoolState())
        return;
    taskMaxThreshold_ = taskMaxThreshold;
}

// 设置最大线程数量
void ThreadPool::setMaxThreadSize(int maxThreadSize)
{
    if (checkPoolState())
        return;
    maxThreadSize_ = maxThreadSize;
}

// 向task队列中提交任务
shared_ptr<Result> ThreadPool::submitTask(shared_ptr<Task> sp)
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
        return std::make_shared<Result>(sp, false);
    }

    // 向task队列中添加任务
    taskQue_.push(sp);
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

    return std::make_shared<Result>(sp);
}

// 线程入口函数
void ThreadPool::threadFunc(int threadId)
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
            task->exec();
        }
        idleThreadSize_++;
        // 记录线程开始空闲的时间
        lastTime = std::chrono::high_resolution_clock().now();
    }
}

bool ThreadPool::checkPoolState()
{
    return isPoolRunning;
}

/* ==============================================================*/
// 线程Thread类方法实现
/*===============================================================*/

Thread::Thread(ThreadFunc threadfunc)
    : func_(threadfunc), threadId_(generateId_++)
{
}

// 启动线程函数
void Thread::start()
{
    thread t(func_, threadId_);
    t.detach();
}

int Thread::generateId_ = 0;

int Thread::getId() const
{
    return threadId_;
}
/* ==============================================================*/
// Result方法实现
/*===============================================================*/

Result::Result(shared_ptr<Task> sp, bool isValid)
    : task_(sp), isValid_(isValid)
{
    task_->setResult(this);
}

// 设置任务执行结果
void Result::setVal(Any any)
{
    any_ = std::move(any);
    sem_.post();
}

// 获取任务执行结果
Any Result::get()
{
    sem_.wait();
    return std::move(any_);
}

/* ==============================================================*/
// Task方法实现
/*===============================================================*/

Task::Task()
    : result_(nullptr)
{
}
// 线程函数中调用的接口
void Task::exec()
{
    result_->setVal(std::move(run()));
}

// 设置Result*
void Task::setResult(Result *res)
{
    result_ = res;
}