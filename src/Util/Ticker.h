#pragma once

#include "Utilities.h"
#include <iostream>

namespace JCToolKit
{
    class Ticker
    {
    public:
        Ticker(uint64_t minTickTime = 0)
        {
            _created = _begin = getCurrentMillisecond();
            _minTickTime = minTickTime;
        }

        ~Ticker()
        {
            uint64_t time = createdTime();
            if (time > _minTickTime)
            {
                std::cout << "Tick time:" << time << "ms" << std::endl;
            }
        }

        
        uint64_t elapsedTime() const
        {
            return getCurrentMillisecond() - _begin;
        }

        uint64_t createdTime() const
        {
            return getCurrentMillisecond() - _created;
        }

        void resetTime()
        {
            _begin = getCurrentMillisecond();
        }

    private:
        uint64_t _minTickTime;
        uint64_t _created;
        uint64_t _begin;
    };

}