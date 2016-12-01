// src/socket.cc
//
// InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
//
// Copyright (c) 2015-2016 University of New Hampshire InterOperability Laboratory
//
// This software is available to you under the terms of the GNU
// General Public License (GPL) Version 2, available from the file
// COPYING in the main directory of this source tree.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/* socket.cc */

#include "socket.h"

#include <memory>

#include <cstring>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/socket.h>

namespace {
using AddressInfoPtr = std::unique_ptr<struct addrinfo, void (*)(struct addrinfo *)>;

AddressInfoPtr getAddressInfo(const char *host, const char *port,
        struct addrinfo *hints)
{
    struct addrinfo *ptr;
    int ret = getaddrinfo(host, port, hints, &ptr);
    if (ret) {
        throw GAIException(ret);
    }
    return AddressInfoPtr(ptr, &freeaddrinfo);
}

int getUnboundSocket(int family, int socktype)
{
    int fd = socket(family, socktype, 0);
    if (fd < 0) {
        throw SocketException(errno);
    }
    return fd;
}

int getBoundSocket(const char *src_host, const char *src_port,
        int family, int socktype)
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = socktype;
    hints.ai_flags = AI_PASSIVE;

    AddressInfoPtr info = getAddressInfo(src_host, src_port, &hints);

    int fd = socket(info->ai_family, info->ai_socktype, 0);
    if (fd < 0) {
        throw SocketException(errno, "socket");
    }

    int val = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (ret < 0) {
        close(fd);
        throw SocketException(errno, "setsockopt SO_REUSEADDR");
    }

    if (socktype == SOCK_STREAM) {
        int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
        if (ret < 0) {
            close(fd);
            throw SocketException(errno, "setsockopt TCP_NODELAY");
        }
    }

    ret = bind(fd, info->ai_addr, info->ai_addrlen);
    if (ret < 0) {
        close(fd);
        throw SocketException(errno, "bind");
    }

    return fd;
}

std::string sockaddr2string(const struct sockaddr *addr, socklen_t addrlen)
{
    std::string result;
    char hostbuf[BUFSIZ];
    char portbuf[BUFSIZ];

    /* First try to get a hostname.  If we get a hostname, we can always
     * format "hostname:port". */
    int ret = getnameinfo(addr, addrlen, hostbuf, BUFSIZ, portbuf, BUFSIZ,
                          NI_NAMEREQD|NI_NUMERICSERV);
    if (!ret) {
        result += hostbuf;
        result += ":";
        result += portbuf;
        return result;
    } else if (ret != EAI_NONAME) {
        throw GAIException(ret);
    }

    /* We couldn't get a hostname so try again asking for the IP address
     * representation.  We then must format appropriately for IPv4 or
     * IPv6. */
    ret = getnameinfo(addr, addrlen, hostbuf, BUFSIZ, portbuf, BUFSIZ,
                          NI_NUMERICHOST|NI_NUMERICSERV);
    if (ret) {
        throw GAIException(ret);
    }

    if (addr->sa_family == AF_INET6) {
        result += "[";
        result += hostbuf;
        result += "]:";
        result += portbuf;
    } else {
        result += hostbuf;
        result += ":";
        result += portbuf;
    }
    return result;
}
}

ListeningSocket::ListeningSocket(const char *src_host, const char *src_port,
        int family, int socktype)
{
    this->fd = getBoundSocket(src_host, src_port, family, socktype);
}

ListeningSocket::~ListeningSocket()
{
    if (this->fd >= 0) {
        close(fd);
    }
}

void ListeningSocket::listen(int backlog)
{
    int ret = ::listen(this->fd, backlog);
    if (ret < 0) {
        throw SocketException(errno, "listen");
    }
}

Socket ListeningSocket::accept(struct sockaddr *addr, socklen_t *addrlen)
{
    int ret = ::accept(this->fd, addr, addrlen);
    if (ret < 0) {
        throw SocketException(errno, "accept");
    }
    return makeAgentSocket(ret);
}

std::string ListeningSocket::getName() const
{
    struct sockaddr_storage storage;
    auto addr = reinterpret_cast<struct sockaddr *>(&storage);
    socklen_t addrlen = sizeof(storage);
    int ret = getsockname(this->fd, addr, &addrlen);
    if (ret < 0) {
        throw SocketException(errno, "getsockname");
    }
    return sockaddr2string(addr, addrlen);
}

Socket ListeningSocket::makeAgentSocket(int fd)
{
    return Socket(fd);
}

Socket::Socket(const char *dest_host, const char *dest_port,
               const char *src_host, const char *src_port,
               int family, int socktype)
        : fd(-1), in_fptr(nullptr), out_fptr(nullptr)
{
    if (src_host || src_port) {
        this->fd = getBoundSocket(src_host, src_port, family, socktype);
    } else {
        this->fd = getUnboundSocket(family, socktype);
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = socktype;

    AddressInfoPtr info = getAddressInfo(src_host, src_port, &hints);

    int ret = connect(this->fd, info->ai_addr, info->ai_addrlen);
    if (ret < 0) {
        close(this->fd);
        throw SocketException(errno);
    }

    this->in_fptr = fdopen(this->fd, "r+");
    if (!this->in_fptr) {
        throw SocketException(errno, "fdopen");
    }
}

Socket::Socket(int fd) : fd(fd), in_fptr(nullptr), out_fptr(nullptr)
{
    this->in_fptr = fdopen(fd, "r");
    if (!this->in_fptr) {
        throw SocketException(errno, "fdopen read half");
    }

    int tmp_fd = dup(fd);
    if (tmp_fd < 0) {
        throw SocketException(errno, "dup");
    }
    this->out_fptr = fdopen(tmp_fd, "w");
    if (!this->out_fptr) {
        close(tmp_fd);
        throw SocketException(errno, "fdopen write");
    }
}

Socket::~Socket()
{
    if (this->in_fptr) {
        fclose(in_fptr);
    }
    if (this->out_fptr) {
        fclose(out_fptr);
    }
}

void Socket::sendLine(std::string line)
{
    int ret = fputs(line.c_str(), this->out_fptr);
    if (ret == EOF) {
        throw SocketException(ferror(this->out_fptr), "fputs");
    }
    fflush(this->out_fptr);
}

std::string Socket::recvLine()
{
    char buf[BUFSIZ];

    char *result = fgets(buf, BUFSIZ, this->in_fptr);
    if (!result) {
        int err = ferror(this->in_fptr);
        if (err) {
            throw SocketException(err, "fgets");
        }
        return {};
    }

    return std::string(buf);
}

std::string Socket::getRemoteName() const
{
    struct sockaddr_storage storage;
    auto addr = reinterpret_cast<struct sockaddr *>(&storage);
    socklen_t addrlen = sizeof(storage);
    int ret = getpeername(this->fd, addr, &addrlen);
    if (ret < 0) {
        throw SocketException(errno, "getpeername");
    }
    return sockaddr2string(addr, addrlen);
}

std::string Socket::getName() const
{
    struct sockaddr_storage storage;
    auto addr = reinterpret_cast<struct sockaddr *>(&storage);
    socklen_t addrlen = sizeof(storage);
    int ret = getsockname(this->fd, addr, &addrlen);
    if (ret < 0) {
        throw SocketException(errno, "getsockname");
    }
    return sockaddr2string(addr, addrlen);
}
