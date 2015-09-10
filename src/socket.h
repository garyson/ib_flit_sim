/* socket.h */

#ifndef SOCKET_H
#define SOCKET_H

#include <cstdio>
#include <cstring>
#include <string>
#include <netdb.h>

class SocketException {
public:
    SocketException(int err, std::string prefix = {})
        : prefix(prefix), err(err) {}

    std::string message() const {
        return prefix + ": " + strerror(err);
    }

private:
    std::string prefix;
    int err;
};

class GAIException {
public:
    GAIException(int err) : err(err) {}

    std::string message() const {
        return gai_strerror(err);
    }

private:
    int err;
};

class ListeningSocket;

class Socket {
public:
    Socket(const char *dest_host, const char *dest_port,
           const char *src_host = nullptr, const char *src_port = nullptr,
           int family = AF_UNSPEC, int socktype = SOCK_STREAM);
    Socket(int fd);
    Socket(Socket &&other) : fd(other.fd), in_fptr(other.in_fptr),
                             out_fptr(other.out_fptr) {
        other.in_fptr = nullptr;
        other.out_fptr = nullptr;
    }
    ~Socket();

    Socket(const Socket &) = delete;
    Socket &operator =(const Socket &) = delete;

    void sendLine(std::string line);
    std::string recvLine();

    std::string getName() const;
    std::string getRemoteName() const;

private:
    int fd;
    FILE *in_fptr;
    FILE *out_fptr;
};

class ListeningSocket {
public:
    ListeningSocket(const char *host = nullptr, const char *port = nullptr,
                    int family = AF_UNSPEC, int socktype = SOCK_STREAM);
    ~ListeningSocket();

    ListeningSocket(const ListeningSocket &) = delete;
    ListeningSocket &operator =(const ListeningSocket &) = delete;

    void listen(int backlog = 0);
    Socket accept(struct sockaddr *addr = nullptr,
                socklen_t *addrlen = nullptr);

    std::string getName() const;

private:
    Socket makeAgentSocket(int fd);
    int fd;
};

#endif
