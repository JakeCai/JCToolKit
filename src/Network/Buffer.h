#pragma once

#include <string>
#include <string.h>
#include <memory>
#include "Util/List.h"
#include "Util/Utilities.h"
#include "Util/ReusePool.h"

namespace JCToolKit
{
    class Buffer : public noncopyable
    {
    public:
        typedef std::shared_ptr<Buffer> Ptr;
        Buffer(){};
        virtual ~Buffer(){};
        //返回数据长度
        virtual char *data() const = 0;
        virtual size_t size() const = 0;

        virtual std::string toString() const
        {
            return std::string(data(), size());
        }

        virtual size_t getCapacity() const
        {
            return size();
        }

    private:
        //对象个数统计
        ObjectStatistic<Buffer> _statistic;
    };

    template <typename C>
    class BufferOffset : public Buffer
    {
    public:
        typedef std::shared_ptr<BufferOffset> Ptr;

        BufferOffset(C data, size_t offset = 0, size_t len = 0) : _data(std::move(data))
        {
            setup(offset, len);
        }

        ~BufferOffset() {}

        char *data() const override
        {
            return const_cast<char *>(_data.data()) + _offset;
        }

        size_t size() const override
        {
            return _size;
        }

        string toString() const override
        {
            return string(data(), size());
        }

    private:
        void setup(size_t offset = 0, size_t len = 0)
        {
            _offset = offset;
            _size = len;
            if (_size <= 0 || _size > _data.size())
            {
                _size = _data.size();
            }
        }

    private:
        C _data;
        size_t _offset;
        size_t _size;
    };

    typedef BufferOffset<std::string> BufferString;

    //指针式缓存对象，
    class BufferRaw : public Buffer
    {
    public:
        using Ptr = std::shared_ptr<BufferRaw>;

        static Ptr create();

        ~BufferRaw() override
        {
            if (_data)
            {
                delete[] _data;
            }
        }
        //在写入数据时请确保内存是否越界
        char *data() const override
        {
            return _data;
        }
        //有效数据大小
        size_t size() const override
        {
            return _size;
        }
        //分配内存大小
        void setCapacity(size_t capacity)
        {
            if (_data)
            {
                do
                {
                    if (capacity > _capacity)
                    {
                        //请求的内存大于当前内存，那么重新分配
                        break;
                    }

                    if (_capacity < 2 * 1024)
                    {
                        //2K以下，不重复开辟内存，直接复用
                        return;
                    }

                    if (2 * capacity > _capacity)
                    {
                        //如果请求的内存大于当前内存的一半，那么也复用
                        return;
                    }
                } while (false);

                delete[] _data;
            }
            _data = new char[capacity];
            _capacity = capacity;
        }
        //设置有效数据大小
        void setSize(size_t size)
        {
            if (size > _capacity)
            {
                throw std::invalid_argument("Buffer::setSize out of range");
            }
            _size = size;
        }
        //赋值数据
        void assign(const char *data, size_t size = 0)
        {
            if (size <= 0)
            {
                size = strlen(data);
            }
            setCapacity(size + 1);
            memcpy(_data, data, size);
            _data[size] = '\0';
            setSize(size);
        }

        size_t getCapacity() const override
        {
            return _capacity;
        }

    protected:
        friend class ReusePool_l<BufferRaw>;

        BufferRaw(size_t capacity = 0)
        {
            if (capacity)
            {
                setCapacity(capacity);
            }
        }

        BufferRaw(const char *data, size_t size = 0)
        {
            assign(data, size);
        }

    private:
        size_t _size = 0;
        size_t _capacity = 0;
        char *_data = nullptr;
        //对象个数统计
        ObjectStatistic<BufferRaw> _statistic;
    };

    class BufferList : public noncopyable
    {
    public:
        typedef std::shared_ptr<BufferList> Ptr;
        BufferList(List<Buffer::Ptr> &list);
        ~BufferList() {}

        bool empty();
        size_t count();
        ssize_t send(int fd, int flags, bool udp);

    private:
        void reOffset(size_t n);
        ssize_t send_l(int fd, int flags, bool udp);

    private:
        size_t _iovec_off = 0;
        size_t _remainSize = 0;
        std::vector<struct iovec> _iovec;
        List<Buffer::Ptr> _pkt_list;
        //对象个数统计
        ObjectStatistic<BufferList> _statistic;
    };

}