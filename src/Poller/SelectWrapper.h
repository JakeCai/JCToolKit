#pragma once

#include <sys/types.h>

namespace JCToolKit
{
    class FdSet
    {
    public:
        FdSet();
        ~FdSet();

        void fdZero();
        void fdSet(int fd);
        void fdClr(int fd);
        bool isSet(int fd);
        void *_ptr;
    };

    int jc_select(int cnt, FdSet *read, FdSet *write, FdSet *err, struct timeval *tv);

}