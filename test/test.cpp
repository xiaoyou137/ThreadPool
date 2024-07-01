#include "threadpool.hpp"

using ull = unsigned long long;

/* class MyTask : public Task
{
public:
    MyTask(int begin, int end)
        : Task(), begin_(begin), end_(end)
    {
    }
    ~MyTask() = default;

    Any run()
    {
        ull sum = 0;
        for (int i = begin_; i <= end_; i++)
        {
            sum += i;
        }
        cout << sum << endl;
        //std::this_thread::sleep_for(std::chrono::seconds(3));
        return sum;
    }

private:
    int begin_;
    int end_;
}; */

ull sum1(int begin, int end)
{
    ull sum = 0;
    for (int i = begin; i <= end; i++)
    {
        sum += i;
    }
    return sum;
}

ull sum2(int begin, int end, ull sum)
{
    for (int i = begin; i <= end; i++)
    {
        sum += i;
    }
    return sum;
}

int main()
{
    /*     {
            ThreadPool p;
            p.setMode(PoolMode::MODE_CACHED);
            p.start();

            auto res1 = p.submitTask(std::make_shared<MyTask>(1, 100000000));
            auto res2 = p.submitTask(std::make_shared<MyTask>(100000001, 200000000));
            auto res3 = p.submitTask(std::make_shared<MyTask>(200000001, 300000000));
            auto res4 = p.submitTask(std::make_shared<MyTask>(200000001, 300000000));
            auto res5 = p.submitTask(std::make_shared<MyTask>(200000001, 300000000));
            auto res6 = p.submitTask(std::make_shared<MyTask>(200000001, 300000000));

            ull sum1 = res1->get().Cast<ull>();
            ull sum2 = res2->get().Cast<ull>();
            ull sum3 = res3->get().Cast<ull>();

            cout << sum1 + sum2 + sum3 << endl;
            getchar();
        } */
    /*     {
            ThreadPool p;
            p.start(4);

            auto res1 = p.submitTask(std::make_shared<MyTask>(1, 100000000));
            // ull sum1 = res1->get().Cast<ull>();
            // res1.get().Cast<ull>();

            // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        } */

    {
        ThreadPool p;
        p.start(4);

        auto res1 = p.submitTask(sum1, 1, 100000000);
        auto res2 = p.submitTask(sum1, 100000001, 200000000);
        auto res3 = p.submitTask(sum2, 1, 100000000, 50000000);
        auto res4 = p.submitTask(sum2, 1, 100000000, 1000000000);

        cout << res1.get() << endl;
        cout << res2.get() << endl;
        cout << res3.get() << endl;
        cout << res4.get() << endl;
        // ull sum1 = res1->get().Cast<ull>();
        // res1.get().Cast<ull>();

        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    cout << "main over" << endl;

    return 0;
}
