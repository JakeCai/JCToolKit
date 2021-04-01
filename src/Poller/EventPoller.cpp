#include "EventPoller.h"
#include "Network/SocketHandler.h"
#include "Util/Utilities.h"
#include "Util/uv_errno.h"
#include "SelectWrapper.h"

#include <sys/epoll.h>

#if !defined(EPOLLEXCLUSIVE)
#define EPOLLEXCLUSIVE 0
#endif

#define EPOLL_SIZE 1024
#define toEpoll(event) (((event)&PollEventRead) ? EPOLLIN : 0) | (((event)&PollEventWrite) ? EPOLLOUT : 0) | (((event)&PollEventError) ? (EPOLLHUP | EPOLLERR) : 0) | (((event)&PollEventLT) ? 0 : EPOLLET)
#define toPoller(epoll_event) (((epoll_event)&EPOLLIN) ? PollEventRead : 0) | (((epoll_event)&EPOLLOUT) ? PollEventWrite : 0) | (((epoll_event)&EPOLLHUP) ? PollEventError : 0) | (((epoll_event)&EPOLLERR) ? PollEventError : 0)

namespace JCToolKit
{
    EventPoller &EventPoller::Instance()
    {
        return *(EventPollerPool::Instance().getFirstPoller());
    }

    EventPoller::EventPoller(ThreadPool::Priority priority)
    {
        _priority = priority;
        SocketHandler::setNoBlocked(_pipe.readFD());
        SocketHandler::setNoBlocked(_pipe.writeFD());

#if defined(HAS_EPOLL)
        _epollFd = epoll_create(EPOLL_SIZE);
        if (_epollFd == -1)
        {
            throw std::runtime_error(StrPrinter() << "创建epoll失败" << get_uv_errmsg(true));
        }
        SocketHandler::setCloExec(_epollFd);
#endif

        _loopThreadID = std::this_thread::get_id();
        if (addEvent(_pipe.readFD(), PollEventRead, [this](int event) { onPipeEvent(); }) == -1)
        {
            throw std::runtime_error("epoll添加管道失败");
        }
    }

    void EventPoller::shutdown()
    {
        async_l([]() {
            throw ExitException();
        },
                false, true);

        if (_loopThread)
        {
            try
            {
                _loopThread->join();
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << '\n';
            }

            delete _loopThread;
            _loopThread = nullptr;
        }
    }

    void EventPoller::wait()
    {
        std::lock_guard<std::mutex> lck(_mtxRunning);
    }

    EventPoller::~EventPoller()
    {
        shutdown();
        wait();

#if defined(HAS_EPOLL)
        if (_epollFd != -1)
        {
            close(_epollFd);
            _epollFd = -1;
        }
#endif
        _loopThreadID = std::this_thread::get_id();
        onPipeEvent();
    }

    int EventPoller::addEvent(int fd, int event, PollEventCallBack callBack)
    {
        if (!callBack)
        {
            return -1;
        }

        if (isCurrentThread())
        {
#if defined(HAS_EPOLL)
            struct epoll_event epollEvent = {0};
            epollEvent.events = (toEpoll(event)) | EPOLLEXCLUSIVE;
            epollEvent.data.fd = fd;
            int ret = epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &epollEvent);
            if (ret == 0)
            {
                _eventMap.emplace(fd, std::make_shared<PollEventCallBack>(std::move(callBack)));
            }
            return ret;
#else
            PollRecord::Ptr record(new PollRecord);
            record->event = event;
            record->callBack = std::move(callBack);
            _eventMap.emplace(fd, record);
            return 0;
#endif
        }

        async([this, fd, event, callBack]() {
            addEvent(fd, event, std::move(const_cast<PollEventCallBack &>(callBack)));
        });

        return 0;
    }

    int EventPoller::deleteEvent(int fd, PollDeleteCallBack callBack)
    {
        if (!callBack)
        {
            callBack = [](bool success) {};
        }

        if (isCurrentThread())
        {
#if defined(HAS_EPOLL)
            bool success = epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL) == 0 && _eventMap.erase(fd) > 0;
            callBack(success);
            return success ? 0 : -1;
#else
            callBack(_eventMap.erase(fd));
            return 0;
#endif
        }

        async([this, fd, callBack]() {
            deleteEvent(fd, std::move(const_cast<PollDeleteCallBack &>(callBack)));
        });

        return 0;
    }

    int EventPoller::modifyEvent(int fd, int event)
    {
#if defined(HAS_EPOLL)
        struct epoll_event epollEvent = {0};
        epollEvent.events = toEpoll(event);
        epollEvent.data.fd = fd;
        return epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &epollEvent);
#else
        if (isCurrentThread())
        {
            auto it = _eventMap.find(fd);
            if (it != _eventMap.end())
            {
                it->second->event = event;
            }
            return 0;
        }
        async([this, fd, event]() {
            modifyEvent(fd, event);
        });
        return 0;
#endif //HAS_EPOLL
    }

    Operation::Ptr EventPoller::async(OperationFunction op, bool maySync)
    {
        return async_l(std::move(op), maySync, false);
    }

    Operation::Ptr EventPoller::asyncFirst(OperationFunction op, bool maySync)
    {
        return async_l(std::move(op), maySync, true);
    }

    Operation::Ptr EventPoller::async_l(OperationFunction op, bool maySync, bool first)
    {
        if (maySync && isCurrentThread())
        {
            op();
            return nullptr;
        }

        auto ret = std::make_shared<Operation>(std::move(op));
        {
            std::lock_guard<std::mutex> lck(_mtxOperation);
            if (first)
            {
                _operationList.emplace_front(op);
            }
            else
            {
                _operationList.emplace_back(op);
            }
        }

        _pipe.write("", 1);
        return ret;
    }

    bool EventPoller::isCurrentThread()
    {
        return _loopThreadID == std::this_thread::get_id();
    }

    inline void EventPoller::onPipeEvent()
    {
        char buffer[1024];
        int error = 0;
        do
        {
            if (_pipe.read(buffer, sizeof(buffer)) > 0)
            {
                continue;
            }
            error = get_uv_error(true);
        } while (error != UV_EAGAIN);

        decltype(_operationList) _swapList;
        {
            std::lock_guard<std::mutex> lck(_mtxOperation);
            _swapList.swap(_operationList);
        }

        _swapList.for_each([&](const Operation::Ptr &operation) {
            try
            {
                (*operation)();
            }
            catch (ExitException &)
            {
                _exitFlag = true;
            }
            catch (std::exception &ex)
            {
                printf("EventPoller执行异步任务捕获到异常: %s", ex.what());
            }
        });
    }

    static std::mutex s_all_poller_mtx;
    static std::map<std::thread::id, std::weak_ptr<EventPoller>> s_all_poller;

    BufferRaw::Ptr EventPoller::getSharedBuffer()
    {
        auto ret = _sharedBuffer.lock();
        if (!ret)
        {
            //预留一个字节存放\0结尾符
            ret = BufferRaw::create();
            ret->setCapacity(1 + SOCKET_DEFAULT_BUF_SIZE);
            _sharedBuffer = ret;
        }
        return ret;
    }

    //static
    EventPoller::Ptr EventPoller::getCurrentPoller()
    {
        std::lock_guard<std::mutex> lck(s_all_poller_mtx);
        auto it = s_all_poller.find(std::this_thread::get_id());
        if (it == s_all_poller.end())
        {
            return nullptr;
        }
        return it->second.lock();
    }

    void EventPoller::runLoop(bool blocked, bool registSelf)
    {
        if (blocked)
        {
            ThreadPool::setPriority(_priority);
            std::lock_guard<std::mutex> lck(_mtxRunning);
            _loopThreadID = std::this_thread::get_id();
            if (registSelf)
            {
                std::lock_guard<std::mutex> lck(s_all_poller_mtx);
                s_all_poller[_loopThreadID] = shared_from_this();
            }
            _semWithRunStarted.post();
            _exitFlag = false;
            uint64_t minDelay;

#if defined(HAS_EPOLL)
            struct epoll_event events[EPOLL_SIZE];
            while (!_exitFlag)
            {
                minDelay = getMinDelay();
                startSleep();
                int ret = epoll_wait(_epollFd, events, EPOLL_SIZE, minDelay ? minDelay : -1);
                wakeUp();
                if (ret <= 0)
                {
                    continue;
                }
                for (int i = 0; i < ret; ++i)
                {
                    struct epoll_event &event = events[i];
                    int fd = event.data.fd;
                    auto it = _eventMap.find(fd);
                    if (it == _eventMap.end())
                    {
                        epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
                        continue;
                    }
                    auto callBack = it->second;
                    try
                    {
                        (*callBack)(toPoller(event.events));
                    }
                    catch (std::exception &ex)
                    {
                        printf("EventPoller执行事件回调捕获到异常: %s \n", ex.what());
                    }
                }
            }
#else
            int ret, maxFd;
            FdSet set_read, set_write, set_err;
            List<PollRecord::Ptr> callbackList;
            struct timeval tv;

            while (!_exitFlag)
            {
                tv.tv_sec = (decltype(tv.tv_sec))(minDelay / 1000);
                tv.tv_usec = 1000 * (minDelay % 1000);

                set_read.fdZero();
                set_write.fdZero();
                set_err.fdZero();
                maxFd = 0;

                for (auto &record : _eventMap)
                {
                    if (record.first > maxFd)
                    {
                        maxFd = record.first;
                    }
                    if (record.second->event & PollEventRead)
                    {
                        set_read.fdSet(record.first);
                    }
                    if (record.second->event & PollEventWrite)
                    {
                        set_write.fdSet(record.first);
                    }
                    if (record.second->event & PollEventError)
                    {
                        set_err.fdSet(record.first);
                    }
                }

                startSleep();
                ret = jc_select(maxFd + 1, &set_read, &set_write, &set_err, minDelay ? &tv : NULL);
                wakeUp();

                if (ret <= 0)
                {
                    continue;
                }

                for (auto &record : _eventMap)
                {
                    int event = 0;
                    if (set_read.isSet(record.first))
                    {
                        event |= PollEventRead;
                    }
                    if (set_write.isSet(record.first))
                    {
                        event |= PollEventWrite;
                    }
                    if (set_err.isSet(record.first))
                    {
                        event |= PollEventError;
                    }
                    if (event != 0)
                    {
                        record.second->attach = event;
                        callbackList.emplace_back(record.second);
                    }
                }

                callbackList.for_each([](PollRecord::Ptr &record) {
                    try
                    {
                        record->callBack(record->attach);
                    }
                    catch (std::exception &ex)
                    {
                        printf("EventPoller执行事件回调捕获到异常: %s \n", ex.what());
                    }
                });
                callbackList.clear();
            }
#endif
        }
        else
        {
            _loopThread = new std::thread(&EventPoller::runLoop, this, true, registSelf);
            _semWithRunStarted.wait();
        }
    }

    uint64_t EventPoller::flushDelayOperation(uint64_t nowTime)
    {
        decltype(_delayOperationMap) copyMap;
        copyMap.swap(_delayOperationMap);

        for (auto it = copyMap.begin(); it != copyMap.end() && it->first <= nowTime; it = copyMap.erase(it))
        {
            try
            {
                auto nextDelayTime = (*(it->second))();
                if (nextDelayTime)
                {
                    _delayOperationMap.emplace(nextDelayTime + nowTime, std::move(it->second));
                }
            }
            catch (std::exception &ex)
            {
                printf("EventPoller执行延时任务捕获到异常: %s \n", ex.what());
            }
        }

        copyMap.insert(_delayOperationMap.begin(), _delayOperationMap.end());
        copyMap.swap(_delayOperationMap);

        auto it = _delayOperationMap.begin();
        if (it == _delayOperationMap.end())
        {
            return 0;
        }
        return it->first - nowTime;
    }

    uint64_t EventPoller::getMinDelay()
    {
        auto it = _delayOperationMap.begin();
        if (it == _delayOperationMap.end())
        {
            return 0;
        }
        auto now = getCurrentMillisecond();
        if (it->first > now)
        {
            return it->first - now;
        }
        //执行已到期的任务并刷新休眠延时
        return flushDelayOperation(now);
    }

    DelayOperation::Ptr EventPoller::startDelayOperation(uint64_t delayMs, std::function<uint64_t()> op)
    {
        DelayOperation::Ptr ret = std::make_shared<DelayOperation>(std::move(op));
        auto timeLine = getCurrentMillisecond() + delayMs;
        asyncFirst([timeLine, ret, this]() {
            //异步执行的目的是刷新select或epoll的休眠时间
            _delayOperationMap.emplace(timeLine, ret);
        });
        return ret;
    }

    // MARK: EventPollerPool
    size_t s_pool_size = 0;

    INSTANCE_IMP(EventPollerPool);

    EventPoller::Ptr EventPollerPool::getFirstPoller()
    {
        return std::dynamic_pointer_cast<EventPoller>(_executors.front());
    }

    EventPoller::Ptr EventPollerPool::getPoller()
    {
        auto poller = EventPoller::getCurrentPoller();
        if (_preferCurrentThread && poller)
        {
            return poller;
        }
        return std::dynamic_pointer_cast<EventPoller>(getExecutor());
    }

    void EventPollerPool::preferCurrentThread(bool flag)
    {
        _preferCurrentThread = flag;
    }

    EventPollerPool::EventPollerPool()
    {
        auto size = s_pool_size > 0 ? s_pool_size : std::thread::hardware_concurrency();

        createExecutors([]() {
            EventPoller::Ptr ret(new EventPoller);
            ret->runLoop(false, true);
            return ret;
        }, size);

        printf("创建EventPoller个数: %ld \n", size);
    }

    void EventPollerPool::setPoolSize(size_t size)
    {
        s_pool_size = size;
    }

}
