#include "Buffer.h"

namespace JCToolKit
{
    StatisticImp(Buffer);
    StatisticImp(BufferRaw);
    StatisticImp(BufferList);

    BufferRaw::Ptr BufferRaw::create()
    {
        // static ReusePool<BufferRaw> packet_pool;
        // static onceToken token([]() {
        //     packet_pool.setSize(1024);
        // });
        // auto ret = packet_pool.obtain();
        // ret->setSize(0);
        // return ret;
        return Ptr(new BufferRaw);
    }

    bool BufferList::empty()
    {
        return _iovec_off == _iovec.size();
    }

    size_t BufferList::count()
    {
        return _iovec.size() - _iovec_off;
    }
}