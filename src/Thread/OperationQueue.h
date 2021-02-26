#pragma once

#include <list>
#include <deque>
#include <atomic>
#include <mutex>
#include <functional>
#include "List.h"
#include "Semaphore.h"

namespace JCToolKit
{
    template <typename T>
    class OperationQueue
    {
    public:
        template <typename FUNC>
        void push_back(FUNC &&func)
        {
            {
                std::lock_guard<decltype(_mutex)> lock(_mutex);
                _queue.emplace_back(std::forward<FUNC> func);
            }
            _sem.post();
        }

        template <typename FUNC>
        void push_front(FUNC &&func)
        {
            {
                std::lock_guard<decltype(_mutex)> lock(_mutex);
                _queue.emplace_front(std::forward<FUNC> func);
            }
            _sem.post();
        }

        void push_exit(size_t n)
        {
            _sem.post(n);
        }

        bool get_operation(T &op)
        {
            _sem.wait();
            lock_guard<decltype(_mutex)> lock(_mutex);
            if (_queue.size() == 0)
            {
                return false;
            }
            op = std::move(_queue.front());
            _queue.pop_front();
            return true;
        }

        size_t size() const
        {
            lock_guard<decltype(_mutex)> lock(_mutex);
            return _queue.size();
        }

    private:
        List<T> _queue;
        mutable std::mutex _mutex;
        Semaphore _sem;
    };

}