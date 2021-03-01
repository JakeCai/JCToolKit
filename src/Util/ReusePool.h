#pragma once

#include <mutex>
#include <deque>
#include <memory>
#include <atomic>
#include <functional>
#include <unordered_set>
#include "List.h"

namespace JCToolKit
{
    template <typename T>
    class ReusePool_l;
    template <typename T>
    class ReusePool;

    template <typename T>
    class shared_ptr_imp : public std::shared_ptr<T>
    {
    public:
        shared_ptr_imp() {}
        shared_ptr_imp(T *ptr, const std::weak_ptr<ReusePool_l<T> > &weakPool, std::shared_ptr<std::atomic_bool> quit);

        void quit(bool flag = true)
        {
            if (_quit)
            {
                *_quit = flag;
            }
        }

    private:
        std::shared_ptr<std::atomic_bool> _quit;
    };

    template <typename T>
    class ReusePool_l : public std::enable_shared_from_this<ReusePool_l<T> >
    {
    public:
        typedef shared_ptr_imp<T> ValuePtr;
        friend class shared_ptr_imp<T>;
        friend class ReusePool<T>;

        ReusePool_l()
        {
            _alloctor = []() -> T * {
                return new T();
            };
        }

        template <typename... ArgTypes>
        ReusePool_l(ArgTypes &&...args)
        {
            _alloctor = [args...]() -> T * {
                return new T(args...);
            };
        }

        ~ReusePool_l()
        {
            _objs.for_each([](T *ptr) {
                delete ptr;
            });
        }

        void setSize(size_t size)
        {
            _poolSize = size;
        }

        ValuePtr obtain()
        {
            T *ptr;
            auto flag = _flag.test_and_set();
            if (!flag)
            {
                if (_objs.size() == 0)
                {
                    ptr = _alloctor();
                }
                else
                {
                    ptr = _objs.front();
                    _objs.pop_front();
                }
                _flag.clear();
            }
            else
            {
                ptr = _alloctor();
            }
            return ValuePtr(ptr, _weakSelf, std::make_shared<std::atomic_bool>(false));
        }
    private:
        size_t _poolSize = 8;
        List<T *> _objs;
        std::function<T *(void)> _alloctor;
        std::atomic_flag _flag{false};
        std::weak_ptr<ReusePool_l> _weakSelf;

    private:
        void recycle(T *obj)
        {
            auto flag = _flag.test_and_set();
            if (!flag)
            {
                if (_objs.size() >= _poolSize)
                {
                    delete obj;
                }
                else
                {
                    _objs.emplace_back(obj);
                }
                _flag.clear();
            }
            else
            {
                delete obj;
            }
        }

        void setup()
        {
            _weakSelf = this->shared_from_this();
        }
    };

    template <typename T>
    class ReusePool
    {
    public:
        typedef shared_ptr_imp<T> ValuePtr;
        ReusePool()
        {
            _pool.reset(new ReusePool_l<T>());
            _pool->setup();
        }

        template <typename... ArgTypes>
        ReusePool(ArgTypes &&...args)
        {
            _pool = std::make_shared<ReusePool_l<T> >(std::forward<ArgTypes>(args)...);
            _pool->setup();
        }

        void setSize(size_t size)
        {
            _pool->setSize(size);
        }

        ValuePtr obtain()
        {
            return _pool->obtain();
        }

    private:
        std::shared_ptr<ReusePool_l<T> > _pool;
    };

    template <typename T>
    shared_ptr_imp<T>::shared_ptr_imp(T *ptr,
                                      const std::weak_ptr<ReusePool_l<T> > &weakPool,
                                      std::shared_ptr<std::atomic_bool> quit) : std::shared_ptr<T>(ptr, [weakPool, quit](T *ptr) {
                                                                                    auto strongPool = weakPool.lock();
                                                                                    if (strongPool && !(*quit))
                                                                                    {
                                                                                        //循环池还在并且不放弃放入循环池
                                                                                        strongPool->recycle(ptr);
                                                                                    }
                                                                                    else
                                                                                    {
                                                                                        delete ptr;
                                                                                    }
                                                                                }),
                                                                                _quit(std::move(quit)) {}
}