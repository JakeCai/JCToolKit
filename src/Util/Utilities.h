#pragma once

#include <functional>
#include <type_traits>
#include <sstream>
#include <ctime>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>
#if defined(_WIN32)
#include <WinSock2.h>
#pragma comment(lib, "WS2_32")
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stddef.h>
#endif // defined(_WIN32)

#define INSTANCE_IMP(class_name, ...)                                               \
    class_name &class_name::Instance()                                              \
    {                                                                               \
        static std::shared_ptr<class_name> s_instance(new class_name(__VA_ARGS__)); \
        static class_name &s_insteanc_ref = *s_instance;                            \
        return s_insteanc_ref;                                                      \
    }

namespace JCToolKit
{
    class noncopyable
    {
    protected:
        noncopyable() {}
        ~noncopyable() {}

    private:
        noncopyable(const noncopyable &that) = delete;
        noncopyable(noncopyable &&that) = delete;
        noncopyable &operator=(const noncopyable &that) = delete;
        noncopyable &operator=(noncopyable &&that) = delete;
    };

    class onceToken
    {
    public:
        typedef std::function<void(void)> task;

        template <typename FUNC>
        onceToken(const FUNC &onConstructed, std::function<void(void)> onDestructed = nullptr)
        {
            onConstructed();
            _onDestructed = std::move(onDestructed);
        }

        onceToken(std::nullptr_t, std::function<void(void)> onDestructed = nullptr)
        {
            _onDestructed = std::move(onDestructed);
        }

        ~onceToken()
        {
            if (_onDestructed)
            {
                _onDestructed();
            }
        }

    private:
        onceToken() = delete;
        onceToken(const onceToken &) = delete;
        onceToken(onceToken &&) = delete;
        onceToken &operator=(const onceToken &) = delete;
        onceToken &operator=(onceToken &&) = delete;

    private:
        task _onDestructed;
    };

    template <class T>
    class ObjectStatistic
    {
    public:
        ObjectStatistic()
        {
            ++getCounter();
        }

        ~ObjectStatistic()
        {
            --getCounter();
        }

        static size_t count()
        {
            return getCounter().load();
        }

    private:
        static std::atomic<size_t> &getCounter();
    };

#define StatisticImp(Type)                              \
    template <>                                         \
    atomic<size_t> &ObjectStatistic<Type>::getCounter() \
    {                                                   \
        static atomic<size_t> instance(0);              \
        return instance;                                \
    }

    class StrPrinter : public std::string
    {
    public:
        StrPrinter() {}

        template <typename T>
        StrPrinter &operator<<(T &&data)
        {
            _stream << std::forward<T>(data);
            this->std::string::operator=(_stream.str());
            return *this;
        }

        std::string operator<<(std::ostream &(*f)(std::ostream &)) const
        {
            return *this;
        }

    private:
        std::stringstream _stream;
    };

#define StatisticImp(Type)                              \
    template <>                                         \
    std::atomic<size_t> &ObjectStatistic<Type>::getCounter() \
    {                                                   \
        static std::atomic<size_t> instance(0);              \
        return instance;                                \
    }

    uint64_t getCurrentMillisecond(bool isSystemTime = false);

    uint64_t getCurrentMicrosecond(bool isSystemTime = false);

}
