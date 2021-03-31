#include <fcntl.h>
#include "SocketHandler.h"
#include "Util/Utilities.h"
#include "Util/uv_errno.h"
#include <mutex>

namespace JCToolKit
{

    std::string SocketHandler::inet_ntoa(struct in_addr &addr)
    {
        char buf[20];
        unsigned char *p = (unsigned char *)&(addr);
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
        return buf;
    }

    int SocketHandler::setCloseWait(int sock, int second = 0)
    {
        linger m_slinger;
        m_slinger.l_onoff = (second > 0);
        m_slinger.l_linger = second;

        int ret = setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&m_slinger, sizeof(linger));
        if (ret == -1)
        {
            printf("设置 SO_LINGER 失败 \n");
        }
        return ret;
    }

    int SocketHandler::setNoDelay(int sock, bool on)
    {
        int opt = on ? 1 : 0;
        int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, static_cast<socklen_t>(sizeof(opt)));
        if (ret == -1)
        {
            printf("设置 NoDelay 失败 \n");
        }
        return ret;
    }

    int SocketHandler::setReuseable(int sock, bool on)
    {
        int opt = on ? 1 : 0;
        int ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, static_cast<socklen_t>(sizeof(opt)));
        if (ret == -1)
        {
            printf("设置 SO_REUSEADDR 失败 \n");
        }
        return ret;
    }

    int SocketHandler::setBroadcast(int sockFd, bool on)
    {
        int opt = on ? 1 : 0;
        int ret = setsockopt(sockFd, SOL_SOCKET, SO_BROADCAST, (char *)&opt, static_cast<socklen_t>(sizeof(opt)));
        if (ret == -1)
        {
            printf("设置 SO_BROADCAST 失败! \n");
        }
        return ret;
    }

    int SocketHandler::setKeepAlive(int sockFd, bool on)
    {
        int opt = on ? 1 : 0;
        int ret = setsockopt(sockFd, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, static_cast<socklen_t>(sizeof(opt)));
        if (ret == -1)
        {
            printf("设置 SO_KEEPALIVE 失败! \n");
        }
        return ret;
    }

    int SocketHandler::setCloExec(int fd, bool on)
    {
        int flags = fcntl(fd, F_GETFD);
        if (flags == -1)
        {
            printf("设置 FD_CLOEXEC 失败! \n");
            return -1;
        }
        if (on)
        {
            flags |= FD_CLOEXEC;
        }
        else
        {
            int cloexec = FD_CLOEXEC;
            flags &= ~cloexec;
        }
        int ret = fcntl(fd, F_SETFD, flags);
        if (ret == -1)
        {
            printf("设置 FD_CLOEXEC 失败! \n");
            return -1;
        }
        return ret;
    }

    int SocketHandler::setNoSigpipe(int sd)
    {
#if defined(SO_NOSIGPIPE)
        int set = 1;
        auto ret = setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, (char *)&set, sizeof(int));
        if (ret == -1)
        {
            printf("设置 SO_NOSIGPIPE 失败! \n");
        }
        return ret;
#else
        return -1;
#endif
    }

    int SocketHandler::setNoBlocked(int sock, bool noblock)
    {
        int ul = noblock;                    //defined(_WIN32)
        int ret = ioctl(sock, FIONBIO, &ul); //设置为非阻塞模式
        if (ret == -1)
        {
            printf("设置非阻塞失败! \n");
        }

        return ret;
    }

    int SocketHandler::setRecvBuf(int sock, int size)
    {
        int ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));
        if (ret == -1)
        {
            printf("设置接收缓冲区失败! \n");
        }
        return ret;
    }
    int SocketHandler::setSendBuf(int sock, int size)
    {
        int ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
        if (ret == -1)
        {
            printf("设置发送缓冲区失败!\n");
        }
        return ret;
    }

    class DNSCache
    {
    public:
        static DNSCache &instance()
        {
            static DNSCache instance;
            return instance;
        }

        bool getDomainIP(const char *host, sockaddr &addr, int expireInterval = 60)
        {
            DNSUnit unit;
            auto hasIP = getCacheDomainIP(host, unit, expireInterval);
            if (!hasIP)
            {
                hasIP = getSystemDomainIP(host, unit._addr);
                if (hasIP)
                {
                    setCacheDomainIP(host, unit);
                }
            }
            if (hasIP)
            {
                addr = unit._addr;
            }
            return hasIP;
        }

    private:
        DNSCache();
        ~DNSCache();

        class DNSUnit
        {
        public:
            sockaddr _addr;
            time_t _create_time;
        };

        bool getCacheDomainIP(const char *host, DNSUnit &unit, int expireInterval)
        {
            std::lock_guard<std::mutex> lck(_mtx);
            auto it = _DNSMap.find(host);
            if (it == _DNSMap.end())
            {
                return false;
            }
            if (it->second._create_time + expireInterval < time(NULL))
            {
                _DNSMap.erase(it);
                return false;
            }
            unit = it->second;
            return true;
        }

        void setCacheDomainIP(const char *host, DNSUnit &unit)
        {
            std::lock_guard<std::mutex> lck(_mtx);
            unit._create_time = time(NULL);
            _DNSMap[host] = unit;
        }

        bool getSystemDomainIP(const char *host, sockaddr &addr)
        {
            struct addrinfo *answer = nullptr;
            //阻塞式dns解析，可能被打断
            int ret = -1;
            do
            {
                ret = getaddrinfo(host, NULL, NULL, &answer);
            } while (ret == -1 && get_uv_error(true) == UV_EINTR);

            if (!answer)
            {
                printf("域名解析失败:%s  \n", host);
                return false;
            }
            addr = *(answer->ai_addr);
            freeaddrinfo(answer);
            return true;
        }

    private:
        std::mutex _mtx;
        std::unordered_map<std::string, DNSUnit> _DNSMap;
    };

    bool SocketHandler::getDomainIP(const char *host, uint16_t port, sockaddr &addr)
    {
        bool hasIP = DNSCache::instance().getDomainIP(host, addr);
        if (hasIP)
        {
            ((sockaddr_in *)&addr)->sin_port = htons(port);
        }
        return hasIP;
    }

    int SocketHandler::connect(const char *host, uint16_t port, bool isAsync = true, const char *localIp = "0.0.0.0", uint16_t localPort = (uint16_t)0U)
    {
        sockaddr addr;
        if (!DNSCache::instance().getDomainIP(host, addr))
        {
            return -1;
        }

        ((sockaddr_in *)&addr)->sin_port = htons(port);

        int sockFD = (int)socket(addr.sa_family, SOCK_STREAM, IPPROTO_TCP);
        if (sockFD < 0)
        {
            printf("创建socket失败  \n");
            return -1;
        }

        setReuseable(sockFD);
        setNoSigpipe(sockFD);
        setNoBlocked(sockFD, isAsync);
        setNoDelay(sockFD);
        setSendBuf(sockFD);
        setRecvBuf(sockFD);
        setCloseWait(sockFD);
        setCloExec(sockFD);

        if (bindSock(sockFD, localIp, localPort) == -1)
        {
            close(sockFD);
            return -1;
        }

        if (::connect(sockFD, &addr, sizeof(struct sockaddr) == 0))
        {
            return sockFD;
        }

        if (isAsync && get_uv_error(true) == UV_EAGAIN)
        {
            return sockFD;
        }

        printf("连接主机失败: %s %s %s  \n", localIp, localPort, get_uv_errmsg(true));
        close(sockFD);

        return -1;
    }

    int SocketHandler::listen(const uint16_t port, const char *localIp, int backLog)
    {
        int sockfd = -1;
        if ((sockfd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        {
            printf("创建套接字失败: %s  \n", get_uv_errmsg(true));
            return -1;
        }

        setReuseable(sockfd);
        setNoBlocked(sockfd);
        setCloExec(sockfd);

        if (bindSock(sockfd, localIp, port) == -1)
        {
            close(sockfd);
            return -1;
        }

        //开始监听
        if (::listen(sockfd, backLog) == -1)
        {
            printf("开始监听失败: %s  \n", get_uv_errmsg(true));
            close(sockfd);
            return -1;
        }

        return sockfd;
    }

    int SocketHandler::getSockError(int sockFd)
    {
        int opt;
        socklen_t optLen = static_cast<socklen_t>(sizeof(opt));

        if (getsockopt(sockFd, SOL_SOCKET, SO_ERROR, (char *)&opt, &optLen) < 0)
        {
            return get_uv_error(true);
        }
        else
        {
            return uv_translate_posix_error(opt);
        }
    }

    std::string SocketHandler::get_local_ip(int fd)
    {
        struct sockaddr addr;
        struct sockaddr_in *addr_v4;
        socklen_t addr_len = sizeof(addr);
        //获取local ip and port
        memset(&addr, 0, sizeof(addr));
        if (0 == getsockname(fd, &addr, &addr_len))
        {
            if (addr.sa_family == AF_INET)
            {
                addr_v4 = (sockaddr_in *)&addr;
                return SocketHandler::inet_ntoa(addr_v4->sin_addr);
            }
        }
        return "";
    }

#if defined(__APPLE__)
    template <typename FUN>
    void for_each_netAdapter_apple(FUN &&fun)
    { //type: struct ifaddrs *
        struct ifaddrs *interfaces = NULL;
        struct ifaddrs *adapter = NULL;
        if (getifaddrs(&interfaces) == 0)
        {
            adapter = interfaces;
            while (adapter)
            {
                if (adapter->ifa_addr->sa_family == AF_INET)
                {
                    if (fun(adapter))
                    {
                        break;
                    }
                }
                adapter = adapter->ifa_next;
            }
            freeifaddrs(interfaces);
        }
    }
#endif //defined(__APPLE__)

#if !defined(_WIN32) && !defined(__APPLE__)
    template <typename FUN>
    void for_each_netAdapter_posix(FUN &&fun)
    { //type: struct ifreq *
        struct ifconf ifconf;
        char buf[1024 * 10];
        //初始化ifconf
        ifconf.ifc_len = sizeof(buf);
        ifconf.ifc_buf = buf;
        int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0)
        {
            printf("创建套接字失败: %s \n", get_uv_errmsg(true));
            return;
        }
        if (-1 == ioctl(sockfd, SIOCGIFCONF, &ifconf))
        { //获取所有接口信息
            printf("ioctl 失败: %s \n", get_uv_errmsg(true));
            close(sockfd);
            return;
        }
        close(sockfd);
        //接下来一个一个的获取IP地址
        struct ifreq *adapter = (struct ifreq *)buf;
        for (int i = (ifconf.ifc_len / sizeof(struct ifreq)); i > 0; --i, ++adapter)
        {
            if (fun(adapter))
            {
                break;
            }
        }
    }
#endif //!defined(_WIN32) && !defined(__APPLE__)

    bool check_ip(std::string &address, const std::string &ip)
    {
        if (ip != "127.0.0.1" && ip != "0.0.0.0")
        {
            /*获取一个有效IP*/
            address = ip;
            uint32_t addressInNetworkOrder = htonl(inet_addr(ip.data()));
            if (/*(addressInNetworkOrder >= 0x0A000000 && addressInNetworkOrder < 0x0E000000) ||*/
                (addressInNetworkOrder >= 0xAC100000 && addressInNetworkOrder < 0xAC200000) ||
                (addressInNetworkOrder >= 0xC0A80000 && addressInNetworkOrder < 0xC0A90000))
            {
                //A类私有IP地址：
                //10.0.0.0～10.255.255.255
                //B类私有IP地址：
                //172.16.0.0～172.31.255.255
                //C类私有IP地址：
                //192.168.0.0～192.168.255.255
                //如果是私有地址 说明在nat内部

                /* 优先采用局域网地址，该地址很可能是wifi地址
             * 一般来说,无线路由器分配的地址段是BC类私有ip地址
             * 而A类地址多用于蜂窝移动网络
             */
                return true;
            }
        }
        return false;
    }

    std::string SocketHandler::get_local_ip()
    {
#if defined(__APPLE__)
        std::string address = "127.0.0.1";
        for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
            std::string ip = SocketHandler::inet_ntoa(((struct sockaddr_in *)adapter->ifa_addr)->sin_addr);
            return check_ip(address, ip);
        });
        return address;
#else
        std::string address = "127.0.0.1";
        for_each_netAdapter_posix([&](struct ifreq *adapter) {
            std::string ip = SocketHandler::inet_ntoa(((struct sockaddr_in *)&(adapter->ifr_addr))->sin_addr);
            return check_ip(address, ip);
        });
        return address;
#endif
    }

    std::vector<std::map<std::string, std::string>> SocketHandler::getInterfaceList()
    {
        std::vector<std::map<std::string, std::string>> ret;
#if defined(__APPLE__)
        for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
            std::map<std::string, std::string> obj;
            obj["ip"] = SocketHandler::inet_ntoa(((struct sockaddr_in *)adapter->ifa_addr)->sin_addr);
            obj["name"] = adapter->ifa_name;
            ret.emplace_back(std::move(obj));
            return false;
        });
#else
        for_each_netAdapter_posix([&](struct ifreq *adapter) {
            std::map<std::string, std::string> obj;
            obj["ip"] = SocketHandler::inet_ntoa(((struct sockaddr_in *)&(adapter->ifr_addr))->sin_addr);
            obj["name"] = adapter->ifr_name;
            ret.emplace_back(std::move(obj));
            return false;
        });
#endif
        return ret;
    };

    uint16_t SocketHandler::get_local_port(int fd)
    {
        struct sockaddr addr;
        struct sockaddr_in *addr_v4;
        socklen_t addr_len = sizeof(addr);
        //获取remote ip and port
        if (0 == getsockname(fd, &addr, &addr_len))
        {
            if (addr.sa_family == AF_INET)
            {
                addr_v4 = (sockaddr_in *)&addr;
                return ntohs(addr_v4->sin_port);
            }
        }
        return 0;
    }

    std::string SocketHandler::get_peer_ip(int fd)
    {
        struct sockaddr addr;
        struct sockaddr_in *addr_v4;
        socklen_t addr_len = sizeof(addr);
        //获取remote ip and port
        if (0 == getpeername(fd, &addr, &addr_len))
        {
            if (addr.sa_family == AF_INET)
            {
                addr_v4 = (sockaddr_in *)&addr;
                return SocketHandler::inet_ntoa(addr_v4->sin_addr);
            }
        }
        return "";
    }

    int SocketHandler::bindSock(int sockFd, const char *ifr_ip, uint16_t port)
    {
        struct sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);
        servaddr.sin_addr.s_addr = inet_addr(ifr_ip);
        bzero(&(servaddr.sin_zero), sizeof servaddr.sin_zero);
        //绑定监听
        if (::bind(sockFd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
        {
            printf("绑定套接字失败: %s \n", get_uv_errmsg(true));
            return -1;
        }
        return 0;
    }

    int SocketHandler::bindUdpSock(const uint16_t port, const char *localIp)
    {
        int sockfd = -1;
        if ((sockfd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        {
            printf("创建套接字失败: %s \n", get_uv_errmsg(true));
            return -1;
        }

        setReuseable(sockfd);
        setNoSigpipe(sockfd);
        setNoBlocked(sockfd);
        setSendBuf(sockfd);
        setRecvBuf(sockfd);
        setCloseWait(sockfd);
        setCloExec(sockfd);

        if (bindSock(sockfd, localIp, port) == -1)
        {
            close(sockfd);
            return -1;
        }
        return sockfd;
    }

    uint16_t SocketHandler::get_peer_port(int fd)
    {
        struct sockaddr addr;
        struct sockaddr_in *addr_v4;
        socklen_t addr_len = sizeof(addr);
        //获取remote ip and port
        if (0 == getpeername(fd, &addr, &addr_len))
        {
            if (addr.sa_family == AF_INET)
            {
                addr_v4 = (sockaddr_in *)&addr;
                return ntohs(addr_v4->sin_port);
            }
        }
        return 0;
    }

    std::string SocketHandler::get_ifr_ip(const char *ifrName)
    {
#if defined(__APPLE__)
        std::string ret;
        for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
            if (strcmp(adapter->ifa_name, ifrName) == 0)
            {
                ret = SocketHandler::inet_ntoa(((struct sockaddr_in *)adapter->ifa_addr)->sin_addr);
                return true;
            }
            return false;
        });
        return ret;
#else
        std::string ret;
        for_each_netAdapter_posix([&](struct ifreq *adapter) {
            if (strcmp(adapter->ifr_name, ifrName) == 0)
            {
                ret = SocketHandler::inet_ntoa(((struct sockaddr_in *)&(adapter->ifr_addr))->sin_addr);
                return true;
            }
            return false;
        });
        return ret;
#endif
    }

    std::string SocketHandler::get_ifr_name(const char *localIp)
    {
#if defined(__APPLE__)
        std::string ret = "en0";
        for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
            std::string ip = SocketHandler::inet_ntoa(((struct sockaddr_in *)adapter->ifa_addr)->sin_addr);
            if (ip == localIp)
            {
                ret = adapter->ifa_name;
                return true;
            }
            return false;
        });
        return ret;
#else
        std::string ret = "en0";
        for_each_netAdapter_posix([&](struct ifreq *adapter) {
            std::string ip = SocketHandler::inet_ntoa(((struct sockaddr_in *)&(adapter->ifr_addr))->sin_addr);
            if (ip == localIp)
            {
                ret = adapter->ifr_name;
                return true;
            }
            return false;
        });
        return ret;
#endif
    }

    std::string SocketHandler::get_ifr_mask(const char *ifrName)
    {
#if defined(__APPLE__)
        std::string ret;
        for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
            if (strcmp(ifrName, adapter->ifa_name) == 0)
            {
                ret = SocketHandler::inet_ntoa(((struct sockaddr_in *)adapter->ifa_netmask)->sin_addr);
                return true;
            }
            return false;
        });
        return ret;
#else
        int sockFd;
        struct ifreq ifr_mask;
        sockFd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockFd == -1)
        {
            printf("创建套接字失败: %s \n", get_uv_errmsg(true));
            return "";
        }
        memset(&ifr_mask, 0, sizeof(ifr_mask));
        strncpy(ifr_mask.ifr_name, ifrName, sizeof(ifr_mask.ifr_name) - 1);
        if ((ioctl(sockFd, SIOCGIFNETMASK, &ifr_mask)) < 0)
        {
            printf("ioctl 失败: %s %s \n", ifrName, get_uv_errmsg(true));
            close(sockFd);
            return "";
        }
        close(sockFd);
        return SocketHandler::inet_ntoa(((struct sockaddr_in *)&(ifr_mask.ifr_netmask))->sin_addr);
#endif // defined(_WIN32)
    }

    std::string SocketHandler::get_ifr_brdaddr(const char *ifrName)
    {
#if defined(__APPLE__)
        string ret;
        for_each_netAdapter_apple([&](struct ifaddrs *adapter) {
            if (strcmp(ifrName, adapter->ifa_name) == 0)
            {
                ret = SocketHandler::inet_ntoa(((struct sockaddr_in *)adapter->ifa_broadaddr)->sin_addr);
                return true;
            }
            return false;
        });
        return ret;
#else
        int sockFd;
        struct ifreq ifr_mask;
        sockFd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockFd == -1)
        {
            printf("创建套接字失败: %s \n", get_uv_errmsg(true));
            return "";
        }
        memset(&ifr_mask, 0, sizeof(ifr_mask));
        strncpy(ifr_mask.ifr_name, ifrName, sizeof(ifr_mask.ifr_name) - 1);
        if ((ioctl(sockFd, SIOCGIFBRDADDR, &ifr_mask)) < 0)
        {
            printf("ioctl 失败: %s \n", get_uv_errmsg(true));
            close(sockFd);
            return "";
        }
        close(sockFd);
        return SocketHandler::inet_ntoa(((struct sockaddr_in *)&(ifr_mask.ifr_broadaddr))->sin_addr);
#endif
    }

#define ip_addr_netcmp(addr1, addr2, mask) (((addr1) & (mask)) == ((addr2) & (mask)))
    bool SocketHandler::in_same_lan(const char *myIp, const char *dstIp)
    {
        std::string mask = get_ifr_mask(get_ifr_name(myIp).data());
        return ip_addr_netcmp(inet_addr(myIp), inet_addr(dstIp), inet_addr(mask.data()));
    }

    static void clearMulticastAllSocketOption(int socket)
    {
#if defined(IP_MULTICAST_ALL)
        // This option is defined in modern versions of Linux to overcome a bug in the Linux kernel's default behavior.
        // When set to 0, it ensures that we receive only packets that were sent to the specified IP multicast address,
        // even if some other process on the same system has joined a different multicast group with the same port number.
        int multicastAll = 0;
        (void)setsockopt(socket, IPPROTO_IP, IP_MULTICAST_ALL, (void *)&multicastAll, sizeof multicastAll);
        // Ignore the call's result.  Should it fail, we'll still receive packets (just perhaps more than intended)
#endif
    }

    int SocketHandler::setMultiTTL(int sockFd, uint8_t ttl)
    {
        int ret = -1;
#if defined(IP_MULTICAST_TTL)
        ret = setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl));
        if (ret == -1)
        {
            printf("设置 IP_MULTICAST_TTL 失败! \n");
        }
#endif
        clearMulticastAllSocketOption(sockFd);
        return ret;
    }

    int SocketHandler::setMultiIF(int sockFd, const char *strLocalIp)
    {
        int ret = -1;
#if defined(IP_MULTICAST_IF)
        struct in_addr addr;
        addr.s_addr = inet_addr(strLocalIp);
        ret = setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&addr, sizeof(addr));
        if (ret == -1)
        {
            printf("设置 IP_MULTICAST_IF 失败! \n");
        }
#endif
        clearMulticastAllSocketOption(sockFd);
        return ret;
    }

    int SocketHandler::setMultiLOOP(int sockFd, bool bAccept)
    {
        int ret = -1;
#if defined(IP_MULTICAST_LOOP)
        uint8_t loop = bAccept;
        ret = setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loop, sizeof(loop));
        if (ret == -1)
        {
            printf("设置 IP_MULTICAST_LOOP 失败! \n");
        }
#endif
        clearMulticastAllSocketOption(sockFd);
        return ret;
    }

    int SocketHandler::joinMultiAddr(int sockFd, const char *strAddr, const char *strLocalIp)
    {
        int ret = -1;
#if defined(IP_ADD_MEMBERSHIP)
        struct ip_mreq imr;
        imr.imr_multiaddr.s_addr = inet_addr(strAddr);
        imr.imr_interface.s_addr = inet_addr(strLocalIp);
        ret = setsockopt(sockFd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&imr, sizeof(struct ip_mreq));
        if (ret == -1)
        {
            printf("设置 IP_ADD_MEMBERSHIP 失败: %s \n", get_uv_errmsg(true));
        }
#endif
        clearMulticastAllSocketOption(sockFd);
        return ret;
    }

    int SocketHandler::leaveMultiAddr(int sockFd, const char *strAddr, const char *strLocalIp)
    {
        int ret = -1;
#if defined(IP_DROP_MEMBERSHIP)
        struct ip_mreq imr;
        imr.imr_multiaddr.s_addr = inet_addr(strAddr);
        imr.imr_interface.s_addr = inet_addr(strLocalIp);
        ret = setsockopt(sockFd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&imr, sizeof(struct ip_mreq));
        if (ret == -1)
        {
            printf("设置 IP_DROP_MEMBERSHIP 失败: %s \n", get_uv_errmsg(true));
        }
#endif
        clearMulticastAllSocketOption(sockFd);
        return ret;
    }

    template <typename A, typename B>
    static inline void write4Byte(A &&a, B &&b)
    {
        memcpy(&a, &b, sizeof(a));
    }

    int SocketHandler::joinMultiAddrFilter(int sockFd, const char *strAddr, const char *strSrcIp, const char *strLocalIp)
    {
        int ret = -1;
#if defined(IP_ADD_SOURCE_MEMBERSHIP)
        struct ip_mreq_source imr;

        write4Byte(imr.imr_multiaddr, inet_addr(strAddr));
        write4Byte(imr.imr_sourceaddr, inet_addr(strSrcIp));
        write4Byte(imr.imr_interface, inet_addr(strLocalIp));

        ret = setsockopt(sockFd, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (char *)&imr, sizeof(struct ip_mreq_source));
        if (ret == -1)
        {
            printf("设置 IP_ADD_SOURCE_MEMBERSHIP 失败: %s \n", get_uv_errmsg(true));
        }
#endif
        clearMulticastAllSocketOption(sockFd);
        return ret;
    }

    int SocketHandler::leaveMultiAddrFilter(int sockFd, const char *strAddr, const char *strSrcIp, const char *strLocalIp)
    {
        int ret = -1;
#if defined(IP_DROP_SOURCE_MEMBERSHIP)
        struct ip_mreq_source imr;

        write4Byte(imr.imr_multiaddr, inet_addr(strAddr));
        write4Byte(imr.imr_sourceaddr, inet_addr(strSrcIp));
        write4Byte(imr.imr_interface, inet_addr(strLocalIp));

        ret = setsockopt(sockFd, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, (char *)&imr, sizeof(struct ip_mreq_source));
        if (ret == -1)
        {
            printf("设置 IP_DROP_SOURCE_MEMBERSHIP 失败: %s \n", get_uv_errmsg(true));
        }
#endif
        clearMulticastAllSocketOption(sockFd);
        return ret;
    }
}