#pragma once

#include <vector>
#include "OperationExecutor.h"
#include "ThreadGroup.h"
#include "OperationQueue.h"
#include <iostream>

namespace JCToolKit
{
    class ThreadPool : public OperationExecutor
    {
    public:
        enum Priority
        {
            PRIORITY_LOWEST = 0,
            PRIORITY_LOW,
            PRIORITY_NORMAL,
            PRIORITY_HIGH,
            PRIORITY_HIGHEST
        };

        //num:线程池线程个数
        ThreadPool(size_t num = 1,
                   Priority priority = PRIORITY_HIGHEST,
                   bool autoRun = true) : _threadNum(num), _priority(priority)
        {
            if (autoRun)
            {
                start();
            }
        }

        ~ThreadPool()
        {
            shutdown();
            wait();
        }

        void start()
        {
            if (_threadNum <= 0)
            {
                return;
            }
            size_t total = _threadNum - _threadGroup.size();
            for (size_t i = 0; i < _threadNum; ++i)
            {
                _threadGroup.createThread(std::bind(&ThreadPool::run, this));
            }
        }

        Operation::Ptr async(OperationFunction operation, bool maySync = true)
        {
            if (maySync && _threadGroup.isThisThreadIn())
            {
                operation();
                return nullptr;
            }
            auto op = std::make_shared<Operation>(std::move(operation));
            _queue.push_back(op);
            return op;
        }

        Operation::Ptr asyncFirst(OperationFunction operation, bool maySync = true)
        {
            if (maySync && _threadGroup.isThisThreadIn())
            {
                operation();
                return nullptr;
            }
            auto op = std::make_shared<Operation>(std::move(operation));
            _queue.push_front(op);
            return op;
        }

        size_t size()
        {
            return _queue.size();
        }

        static bool setPriority(Priority priority = PRIORITY_NORMAL, std::thread::native_handle_type threadID = 0)
        {
            static int priorityMin = sched_get_priority_min(SCHED_OTHER);
            if (priorityMin == -1)
            {
                return false;
            }

            static int priorityMax = sched_get_priority_max(SCHED_OTHER);
            if (priorityMax == -1)
            {
                return false;
            }

            static int Priorities[] = {priorityMin,
                                       priorityMin + (priorityMax - priorityMin) / 4,
                                       priorityMin + (priorityMax - priorityMin) / 2,
                                       priorityMin + (priorityMax - priorityMin) * 3 / 4,
                                       priorityMax};

            if (threadID == 0)
            {
                threadID = pthread_self();
            }
            struct sched_param params;
            params.sched_priority = Priorities[priority];
            return pthread_setschedparam(threadID, SCHED_OTHER, &params) == 0;
        }

    private:
        void run()
        {
            ThreadPool::setPriority(_priority);
            Operation::Ptr op;
            while (true)
            {
                startSleep();
                if (!_queue.get_operation(op))
                {
                    //空任务，退出线程
                    break;
                }
                wakeUp();
                try
                {
                    (*op)();
                    op = nullptr;
                }
                catch (std::exception &ex)
                {
                    std::cout << "ThreadPool: catch exception" << ex.what() << std::endl;
                }
            }
        }

        void wait()
        {
            _threadGroup.joinAll();
        }

        void shutdown()
        {
            _queue.push_exit(_threadNum);
        }

    private:
        size_t _threadNum;
        OperationQueue<Operation::Ptr> _queue;
        ThreadGroup _threadGroup;
        Priority _priority;
    };

}