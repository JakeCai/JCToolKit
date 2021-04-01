#pragma once

#include <memory>
#include "ThreadPool.h"
#include "Poller/EventPoller.h"

namespace JCToolKit
{
    class WorkThreadPool : public std::enable_shared_from_this<WorkThreadPool>, public OperationExecutorProvider
    {
    public:
        typedef std::shared_ptr<WorkThreadPool> Ptr;
        ~WorkThreadPool(){};

        static WorkThreadPool &Instance();

        static void setPoolSize(int size = 0);

        EventPoller::Ptr getFirstPoller();

        EventPoller::Ptr getPoller();

    private:
        WorkThreadPool();

    private:
        static int s_pool_size;
    };
}