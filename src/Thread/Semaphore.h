#pragma once

#include <condition_variable>
#include <mutex>
#include <atomic>

namespace JCToolKit {
    class Semaphore
    {
    public:
        explicit Semaphore() {
            _count = 0;
        }

        ~Semaphore() {}

        void post(size_t num = 1) {
            std::unique_lock<std::mutex> lock(_mutex);
            _count += num;
            if (num == 1)
            {
                _condition.notify_one();
            } else {
                _condition.notify_all();
            }
            
        }

        void wait() {
            std::unique_lock<std::mutex> lock(_mutex);
            while (_count == 0)
            {
                _condition.wait(lock);
            }
            --_count;
        }

    private:
        size_t _count;
        std::mutex _mutex;
        std::condition_variable_any _condition;
    };
    
}