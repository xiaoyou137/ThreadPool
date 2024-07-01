# ThreadPool

高并发线程池项目

**main branch**: （最终版本）所有实现集中在threadpool.h头文件中，include头文件即可使用。

**version-0.1 branch**: cmake编译成动态库.so文件，生成在lib目录下

# 功能设计实现：

1. main分支（最终版）：使用可变参数模板+引用折叠+完美转发实现submitTask接口，能接受任意数量任意个数的参数。使用future+packaged_task+lambda表达式来设计submitTask的返回值机制。关键代码段如下：
    
    ![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/1.png)
    
2. version-0.1分支：用C++11语言特性实现C++17新特性Any万能类和C++20新特性Semaphore，从而实现Result类和Task类（）来设计submitTask接口，关键代码段如下：
3. 使用vector管理线程对象，使用queue管理任务对象。
4. 基于条件变量和互斥锁实现线程间通信。

# 遇到的问题和解决方案

## 1.项目死锁问题

在线程池析构，回收线程资源时，遇到了死锁问题，问题原因如下：

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/2.png)

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/3.png)

如果主线程的线程池析构函数，和一个刚执行完task的工作线程，遇到了如上图两种调用时序运行的话，即主线程的cvNotEmpty_.notify_all 在 工作线程的cvNotEmpty_.wait之前执行，则造成死锁问题。

**解决方法：**

1. 将主线程的cvNotEmpty_.notify_all放到unique_lock之后
2. 工作线程进行锁+双重检查

如下图

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/4.png)

## 2.window下运行正常，linux却又遇到死锁。

问题：在windows vs下运行正常的程序，放到linux下运行，出现了死锁。

gdb attach调试后发现：Semaphore中的cond_.notify_all()陷入死锁：

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/5.png)

查看源码发现：

windows的vs下的condition_variable的析构函数释放了资源：

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/6.png)

linux下的condition_variable析构函数什么也没做：

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/7.png)

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/8.png)

所以linux下出现死锁。

根本原因是用户代码中，Result对象没有调用get方法阻塞线程，提前析构（早于task任务完成之前）

解决方法：

防卫性编程：

- 给Result添加一个线程安全的atomic_bool类型变量isExit_用来判断对象是否析构，如果已经析构，则方法setVal中不在调用sem_.post(),而是直接return；
- 考虑到Result类需要移动构造，而atomic_bool不可复制不可移动，因此给Result添加一个unique_ptr<atomic_bool>类型的isExit_。

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/9.png)

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/10.png)

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/11.png)

## 3.含有mutex和condition_variable成员的类Semaphore不能移动构造

解决：使用unique_ptr<mutex>代替mutex

## 4.task中有result的裸指针，在result用task作为参数构造时，会设置this到task中的result裸指针，但是在submitTask返回Result时使用了移动构造，因此其后task通过result_裸指针调用setVal时，result_指向的是被移动后的废弃地址。

解决：在Result的移动构造和移动赋值重载函数中，将关联的task中的Result指针重新设置一下

![image text](https://github.com/xiaoyou137/groceries-repo/blob/main/pics/threadpool/12.png)