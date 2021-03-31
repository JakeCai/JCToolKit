#include "SelectWrapper.h"
#include <stddef.h>

namespace JCToolKit
{
    FdSet::FdSet()
    {
        _ptr = new fd_set;
    }

    FdSet::~FdSet()
    {
        delete (fd_set *)_ptr;
    }

    void FdSet::fdZero()
    {
        FD_ZERO((fd_set *)_ptr);
    }

    void FdSet::fdClr(int fd)
    {
        FD_CLR(fd, (fd_set *)_ptr);
    }

    void FdSet::fdSet(int fd)
    {
        FD_SET(fd, (fd_set *)_ptr);
    }

    bool FdSet::isSet(int fd)
    {
        return FD_ISSET(fd, (fd_set *)_ptr);
    }

    int jc_select(int cnt, FdSet *read, FdSet *write, FdSet *err, struct timeval *tv)
    {
        void *rd, *wt, *er;
        rd = read ? read->_ptr : NULL;
        wt = write ? write->_ptr : NULL;
        er = err ? err->_ptr : NULL;
        return ::select(cnt, (fd_set *)rd, (fd_set *)wt, (fd_set *)er, tv);
    }
}