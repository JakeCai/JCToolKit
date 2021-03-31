#include <stdexcept>
#include "Pipe.h"
#include "Network/SocketHandler.h"
#include "Util/uv_errno.h"
#include "Util/Utilities.h"
#include <unistd.h>

#define checkFD(fd)                                                                          \
    if (fd == -1)                                                                            \
    {                                                                                        \
        clearFD();                                                                           \
        throw runtime_error(StrPrinter << "create windows pipe failed:" << get_uv_errmsg()); \
    }

#define closeFD(fd) \
    if (fd != -1)   \
    {               \
        close(fd);  \
        fd = -1;    \
    }

namespace JCToolKit
{
    PipeWrapper::PipeWrapper()
    {
        if (pipe(_pipe_fd) == -1)
        {
            throw std::runtime_error(StrPrinter() << "create posix pipe failed:" << get_uv_errmsg());
        }
        SocketHandler::setNoBlocked(_pipe_fd[0], true);
        SocketHandler::setNoBlocked(_pipe_fd[1], false);
        SocketHandler::setCloExec(_pipe_fd[0]);
        SocketHandler::setCloExec(_pipe_fd[1]);
    }

    void PipeWrapper::clearFD()
    {
        closeFD(_pipe_fd[0]);
        closeFD(_pipe_fd[1]);
    }

    PipeWrapper::~PipeWrapper()
    {
        clearFD();
    }

    int PipeWrapper::write(const void *buf, int n)
    {
        int ret;
        do
        {
            ret = ::write(_pipe_fd[1], buf, n);
        } while (-1 == ret && UV_EINTR == get_uv_error(true));
        return ret;
    }

    int PipeWrapper::read(void *buf, int n)
    {
        int ret;
        do
        {
            ret = ::read(_pipe_fd[0], buf, n);
        } while (-1 == ret && UV_EINTR == get_uv_error(true));
        return ret;
    }
}