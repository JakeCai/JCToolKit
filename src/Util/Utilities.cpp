#include "Utilities.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>

namespace JCToolKit
{
    static inline uint64_t getCurrentMicrosecondOrigin()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    static std::atomic<uint64_t> s_currentMicrosecond(0);
    static std::atomic<uint64_t> s_currentMillisecond(0);
    static std::atomic<uint64_t> s_currentMicrosecond_system(getCurrentMicrosecondOrigin());
    static std::atomic<uint64_t> s_currentMillisecond_system(getCurrentMicrosecondOrigin() / 1000);

    static inline bool initMillisecondThread()
    {
        static std::thread s_thread([]() {
            uint64_t last = getCurrentMicrosecondOrigin();
            uint64_t now;
            uint64_t microsecond = 0;
            while (true)
            {
                now = getCurrentMicrosecondOrigin();
                //记录系统时间戳，可回退
                s_currentMicrosecond_system.store(now, std::memory_order_release);
                s_currentMillisecond_system.store(now / 1000, std::memory_order_release);

                //记录流逝时间戳，不可回退
                int64_t expired = now - last;
                last = now;
                if (expired > 0 && expired < 1000 * 1000)
                {
                    //流逝时间处于0~1000ms之间，那么是合理的，说明没有调整系统时间
                    microsecond += expired;
                    s_currentMicrosecond.store(microsecond, std::memory_order_release);
                    s_currentMillisecond.store(microsecond / 1000, std::memory_order_release);
                }
                else if (expired != 0)
                {
                    // WarnL << "Stamp expired is not abnormal:" << expired;
                }
                
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        });
        static onceToken s_token([]() {
            s_thread.detach();
        });
        return true;
    }

    uint64_t getCurrentMillisecond(bool isSystemTime)
    {
        static bool flag = initMillisecondThread();
        if (isSystemTime)
        {
            return s_currentMillisecond_system.load(std::memory_order_acquire);
        }
        return s_currentMillisecond.load(std::memory_order_acquire);
    }

    uint64_t getCurrentMicrosecond(bool isSystemTime)
    {
        static bool flag = initMillisecondThread();
        if (isSystemTime)
        {
            return s_currentMicrosecond_system.load(std::memory_order_acquire);
        }
        return s_currentMicrosecond.load(std::memory_order_acquire);
    }
}