#include "threadpool.hpp"

int main()
{
    ThreadPool p;
    p.start();

    std::this_thread::sleep_for(std::chrono::seconds(5));
}

