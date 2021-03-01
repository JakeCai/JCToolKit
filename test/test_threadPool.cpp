
#include <signal.h>
#include <functional>
#include <atomic>
#include <thread>
#include <iostream>
#include "Util/Ticker.h"
#include "Thread/ThreadPool.h"

int main()
{
    signal(SIGINT, [](int) {
        exit(0);
    });

    JCToolKit::ThreadPool pool(1,JCToolKit::ThreadPool::PRIORITY_HIGHEST,false);
    std::atomic_size_t count(0);

    JCToolKit::Ticker ticker;
        for (int i = 0 ; i < 1000*10000;++i){
        pool.async([&](){
           if(++count >= 1000*10000){
               std::cout << "执行1000万任务总共耗时:" << ticker.elapsedTime() << "ms" << std::endl;
           }
        });
    }
    std::cout << "1000万任务入队耗时:" << ticker.elapsedTime() << "ms" << std::endl;
    uint64_t  lastCount = 0 ,currentCount = 1;
    ticker.resetTime();

    pool.start();
    while (true){
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        currentCount = count.load();
        std::cout << "每秒执行任务数:" << currentCount - lastCount << std::endl;
        if(currentCount - lastCount == 0){
            break;
        }
        lastCount = currentCount; 
    }
    return 0;

}