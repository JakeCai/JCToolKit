#pragma once

#include <unordered_map>
#include <thread>

namespace JCToolKit {
    class ThreadGroup
    {
    public:
        ThreadGroup() {}
        ~ThreadGroup() {
            _threadMap.clear();
        }

        bool isThisThreadIn() {
            auto threadID = std::this_thread::get_id();
            if (_threadID == threadID)
            {
                return true;
            }
            return _threadMap.find(threadID) != _threadMap.end();
        }

        bool isThreadIn(std::thread* thread) {
            if (!thread)
            {
                return false;
            }
            auto result = _threadMap.find(thread->get_id());
            return result != _threadMap.end();
        }

        template<typename FUNC>
        std::thread* createThread(FUNC &&threadFunction) {
            auto newThread = std::shared_ptr<std::thread>(threadFunction);
            _threadID = newThread->get_id();
            _threadMap[_threadID] = newThread;
            return newThread.get();
        }

        void removeThread(std::thread* thread) {
            auto result = _threadMap.find(thread->get_id());
            if (result != _threadMap.end())
            {
                _threadMap.erase(result);
            }
        }

        void joinAll() {
            if (isThisThreadIn())
            {
                throw std::runtime_error("ThreadGroup: join itself is invaild");
            }
            for (auto &it : _threadMap)
            {
                if (it.second->joinable())
                {
                    it.second->join();
                }
            }
            _threadMap.clear();
        }

        size_t size() {
            return _threadMap.size();
        }

    private:
        std::unordered_map<std::thread::id, std::shared_ptr<std::thread> > _threadMap;
        std::thread::id _threadID;
    };
    
}