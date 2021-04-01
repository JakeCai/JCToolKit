#include "WorkThreadPool.h"
#include "Util/Utilities.h"

namespace JCToolKit
{
    int WorkThreadPool::s_pool_size = 0;

    INSTANCE_IMP(WorkThreadPool);

    EventPoller::Ptr WorkThreadPool::getFirstPoller()
    {
        return std::dynamic_pointer_cast<EventPoller>(_executors.front());
    }

    EventPoller::Ptr WorkThreadPool::getPoller()
    {
        return std::dynamic_pointer_cast<EventPoller>(getExecutor());
    }

    WorkThreadPool::WorkThreadPool()
    {
        auto size = s_pool_size > 0 ? s_pool_size : std::thread::hardware_concurrency();
        createExecutors([]() {
            EventPoller::Ptr ret(new EventPoller(ThreadPool::PRIORITY_LOWEST));
            ret->runLoop(false, false);
            return ret;
        },
                        size);
    }

    void WorkThreadPool::setPoolSize(int size)
    {
        s_pool_size = size;
    }
}
