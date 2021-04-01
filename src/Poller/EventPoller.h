#pragma once

#include <functional>
#include <memory>
#include <map>
#include "Thread/ThreadPool.h"
#include "Thread/OperationExecutor.h"
#include "Thread/Semaphore.h"
#include "Network/Buffer.h"
#include "Pipe.h"

#if defined(__linux__) || defined(__linux)
#define HAS_EPOLL
#endif //__linux__

namespace JCToolKit
{
    typedef enum
    {
        PollEventRead = 1 << 0,  //读事件
        PollEventWrite = 1 << 1, //写事件
        PollEventError = 1 << 2, //错误事件
        PollEventLT = 1 << 3,    //水平触发
    } PollEvent;

    typedef std::function<void(int event)> PollEventCallBack;
    typedef std::function<void(bool success)> PollDeleteCallBack;
    typedef OperationCancelableImp<uint64_t(void)> DelayOperation;

    class EventPoller : public OperationExecutor, public std::enable_shared_from_this<EventPoller>
    {
    public:
        typedef std::shared_ptr<EventPoller> Ptr;
        friend class EventPollerPool;
        friend class WorkThreadPool;
        ~EventPoller();

        static EventPoller &Instance();

        int addEvent(int fd, int event, PollEventCallBack callBack);

        int deleteEvent(int fd, PollDeleteCallBack callBack = nullptr);

        int modifyEvent(int fd, int event);

        Operation::Ptr async(OperationFunction operation, bool maySync = true) override;

        Operation::Ptr asyncFirst(OperationFunction operation, bool maySync = true) override;

        bool isCurrentThread();

        DelayOperation::Ptr startDelayOperation(uint64_t delayMs, std::function<uint64_t()> op);

        static EventPoller::Ptr getCurrentPoller();

        BufferRaw::Ptr getSharedBuffer();

    private:
        EventPoller(ThreadPool::Priority priority = ThreadPool::PRIORITY_HIGHEST);

        void runLoop(bool blocked, bool registSelf);

        void onPipeEvent();

        Operation::Ptr async_l(OperationFunction operation, bool maySync = true, bool first = false);

        void wait();

        void shutdown();

        uint64_t flushDelayOperation(uint64_t nowTime);

        uint64_t getMinDelay();

    private:
        class ExitException : public std::exception
        {
        public:
            ExitException() {}
            ~ExitException() {}
        };

    private:
        bool _exitFlag;
        std::weak_ptr<BufferRaw> _sharedBuffer;

        ThreadPool::Priority _priority;
        std::mutex _mtxRunning;
        std::thread *_loopThread = nullptr;
        std::thread::id _loopThreadID;

        Semaphore _semWithRunStarted;

        PipeWrapper _pipe;

        std::mutex _mtxOperation;
        List<Operation::Ptr> _operationList;

#if defined(HAS_EPOLL)
        int _epollFd = -1;
        std::unordered_map<int, std::shared_ptr<PollEventCallBack>> _eventMap;
#else
        struct PollRecord
        {
            typedef std::shared_ptr<PollRecord> Ptr;
            int event;
            int attach;
            PollEventCallBack callBack;
        };
        std::unordered_map<int, PollRecord::Ptr> _eventMap;
#endif
        std::multimap<uint64_t, DelayOperation::Ptr> _delayOperationMap;
    };

    class EventPollerPool : public std::enable_shared_from_this<EventPollerPool>, public OperationExecutorProvider
    {
    public:
        typedef std::shared_ptr<EventPollerPool> Ptr;
        ~EventPollerPool(){};

        static EventPollerPool &Instance();

        static void setPoolSize(size_t size = 0);

        EventPoller::Ptr getPoller();

        EventPoller::Ptr getFirstPoller();

        void preferCurrentThread(bool isPrefer = true);

    private:
        EventPollerPool();

    private:
        bool _preferCurrentThread = true;
    };

}