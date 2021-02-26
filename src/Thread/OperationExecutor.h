#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <thread>
#include "Semaphore.h"
#include "List.h"
#include "Utilities.h"

namespace JCToolKit
{
    class OperationCancelable : public noncopyable
    {
    public:
        OperationCancelable() = default;
        ~OperationCancelable() = default;
        virtual void cancel() = 0;
    };

    template <class T, class... ArgTypes>
    class OperationCancelableImp;

    template <class T, class... ArgTypes>
    class OperationCancelableImp : public OperationCancelable
    {
    public:
        typedef std::shared_ptr<OperationCancelableImp> Ptr;
        typedef std::function<T(ArgTypes...)> FunctionType;
        ~OperationCancelableImp() = default;

        template <typename FUNC>
        OperationCancelableImp(FUNC &&op)
        {
            _strongOp = std::make_shared<FunctionType>(std::forward<FUNC>(op));
            _weakOp = _strongOp;
        }

        void cancel() override
        {
            _strongOp = nullptr;
        }

        operator bool()
        {
            return _strongOp && *_strongOp;
        }

        void operator=(std::nullptr_t)
        {
            _strongOp = nullptr;
        }

        T operator()(ArgTypes... args) const
        {
            auto strongOp = _weakOp.lock();
            if (strongOp && *strongOp)
            {
                return (*strongOp)(std::forward<ArgTypes>(args)...);
            }
            return defaultValue<T>();
        }

        template <typename T>
        static typename std::enable_if<std::is_void<T>::value, void>::type
        defaultValue() {}

        template <typename T>
        static typename std::enable_if<std::is_pointer<T>::value, T>::type
        defaultValue()
        {
            return nullptr;
        }

        template <typename T>
        static typename std::enable_if<std::is_integral<T>::value, T>::type
        defaultValue()
        {
            return 0;
        }

    private:
        std::shared_ptr<FunctionType> _strongOp;
        std::weak_ptr<FunctionType> _weakOp;
    };

    typedef std::function<void()> OperationFunction;
    typedef OperationCancelableImp<void()> Operation;

    class OperationExecutorProtocol
    {
    public:
        OperationExecutorProtocol() = default;
        virtual ~OperationExecutorProtocol() = default;

        virtual Operation::Ptr async(OperationFunction operation, bool maySync = true) = 0;

        virtual Operation::Ptr asyncFirst(OperationFunction operation, bool maySync = true)
        {
            return async(std::move(operation), maySync);
        }

        void sync(const OperationFunction &operation)
        {
            Semaphore sem;
            auto ret = async([&]() {
                onceToken token(nullptr, [&]() {
                    //通过RAII原理防止抛异常导致不执行这句代码
                    sem.post();
                });
                operation();
            });
            if (ret && *ret)
            {
                sem.wait();
            }
        }

        void syncFirst(const OperationFunction &operation)
        {
            Semaphore sem;
            auto ret = asyncFirst([&]() {
                onceToken token(nullptr, [&]() {
                    //通过RAII原理防止抛异常导致不执行这句代码
                    sem.post();
                });
                operation();
            });
            if (ret && *ret)
            {
                sem.wait();
            }
        }
    };

    class ThreadLoad
    {
    public:
        ThreadLoad(uint64_t maxSize, uint64_t maxUsec)
        {
            _lastSleepTime = _lastWakeTime = getCurrentMicrosecond();
            _maxSize = maxSize;
            _maxUsec = maxUsec;
        }
        ~ThreadLoad();

        void startSleep()
        {
            std::lock_guard<std::mutex> lck(_mutex);
            _sleeping = true;

            auto currentTime = getCurrentMicrosecond();
            auto runTime = currentTime - _lastWakeTime;
            _lastSleepTime = currentTime;

            _timeList.emplace_back(runTime, false);

            if (_timeList.size() > _maxSize)
            {
                _timeList.pop_front();
            }
        }

        void wakeUp()
        {
            std::lock_guard<std::mutex> lck(_mutex);
            _sleeping = false;

            auto currentTime = getCurrentMicrosecond();
            auto runTime = currentTime - _lastSleepTime;
            _lastWakeTime = currentTime;

            _timeList.emplace_back(runTime, true);

            if (_timeList.size() > _maxSize)
            {
                _timeList.pop_front();
            }
        }

        int load()
        {
            std::lock_guard<std::mutex> lck(_mutex);

            uint64_t totalSleepTime = 0;
            uint64_t totalRunTime = 0;

            _timeList.for_each([&](const TimeRecord &record) {
                if (record._isSleep)
                {
                    totalSleepTime += record._time;
                }
                else
                {
                    totalRunTime += record._time;
                }
            });

            if (_sleeping)
            {
                totalSleepTime += (getCurrentMicrosecond() - _lastSleepTime);
            }
            else
            {
                totalRunTime += (getCurrentMicrosecond() - _lastWakeTime);
            }

            auto totalTime = totalRunTime + totalSleepTime;

            while (_timeList.size() != 0 && (totalTime > _maxUsec || _timeList.size() > _maxSize))
            {
                TimeRecord &record = _timeList.front();
                if (record._isSleep)
                {
                    totalSleepTime -= record._time;
                }
                else
                {
                    totalRunTime -= record._time;
                }
                totalTime -= record._time;
                _timeList.pop_front();
            }
            if (totalTime == 0)
            {
                return 0;
            }
            return totalRunTime * 100 / totalTime;
        }

    private:
        class TimeRecord
        {
        public:
            TimeRecord(uint64_t time, bool isSleep)
            {
                _time = time;
                _isSleep = isSleep;
            }

        public:
            uint64_t _time;
            bool _isSleep;
        };

    private:
        uint64_t _lastSleepTime;
        uint64_t _lastWakeTime;
        List<TimeRecord> _timeList;
        bool _sleeping = true;
        uint64_t _maxSize;
        uint64_t _maxUsec;
        std::mutex _mutex;
    };

    class OperationExecutor : public OperationExecutorProtocol, public ThreadLoad
    {
    public:
        typedef std::shared_ptr<OperationExecutor> Ptr;

        OperationExecutor(uint64_t maxSize = 32, uint64_t maxUsec = 2 * 1000 * 1000) : ThreadLoad(maxSize, maxUsec) {}
        ~OperationExecutor() {}
    };

    class OperationExecutorProvider
    {
    public:
        typedef std::shared_ptr<OperationExecutorProvider> Ptr;
        OperationExecutorProvider() {}
        ~OperationExecutorProvider() {}

        OperationExecutor::Ptr getExecutor()
        {
            size_t pos = _pos;
            if (pos >= _executors.size())
            {
                pos = 0;
            }
            auto minLoadExecutor = _executors[pos];
            auto minLoad = minLoadExecutor->load();

            for (size_t i = 0; i < _executors.size(); ++i, ++pos)
            {
                if (pos >= _executors.size())
                {
                    pos = 0;
                }

                auto executor = _executors[pos];
                auto load = executor->load();

                if (minLoad > load)
                {
                    minLoadExecutor = executor;
                    minLoad = load;
                }
                if (minLoad == 0)
                {
                    break;
                }
            }
            _pos = pos;
            return minLoadExecutor;
        }

        std::vector<int> getExecutorLoad()
        {
            std::vector<int> vec(_executors.size());
            int i = 0;
            for (auto &executor : _executors)
            {
                vec[i] = executor->load();
                ++i;
            }
            return vec;
        }

        template <typename FUNC>
        void for_each(FUNC &&func)
        {
            for (auto &executor : _executors)
            {
                func(executor);
            }
        }

    protected:
        template <typename FUNC>
        void createExecutors(FUNC &&func, int threadNum = std::thread::hardware_concurrency())
        {
            for (size_t i = 0; i < threadNum; i++)
            {
                _executors.emplace_back(func());
            }
        }

    private:
        size_t _pos = 0;
        std::vector<OperationExecutor::Ptr> _executors;
    };

}