#include "socket.hpp"
#include "exception.hpp"
#include "types.hpp"
#include "util.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cassert>

namespace my_redis::sockets
{

    TCP_Socket::TCP_Socket(u32 ip, u16 port) : fd(-1), ip(ip), port(port)
    {
        this->fd = socket(AF_INET, SOCK_STREAM, 0);
        if (-1 == fd)
        {
            throw exception::errno_exception{"TCP_Socket::TCP_Socket()", errno};
        }
    }

    TCP_Socket::~TCP_Socket()
    {
        auto result = close(fd);
        if (-1 == result)
        {
            std::fprintf(stderr, "TCP_Socket::~TCP_Socket() %s", util::strerror(errno).c_str());
        }
    }

    namespace
    {
        void bind(i32 fd, sockaddr_in *addr)
        {
            auto result = ::bind(fd, reinterpret_cast<const sockaddr *>(addr), sizeof(*addr));
            if (-1 == result)
            {
                throw exception::errno_exception{"void bindAndListen(const TCP_Socket& socket)", errno};
            }
        }

        void listen(i32 fd)
        {
            auto result = ::listen(fd, SOMAXCONN);
            if (-1 == result)
            {
                throw exception::errno_exception{"void bindAndListen(const TCP_Socket& socket)", errno};
            }
        }
    }  // namespace

    void bindAndListen(const TCP_Socket &socket)
    {
        // Assert that the file descriptor is valid
        assert(socket.fd > 0);

        sockaddr_in addr = {.sin_family = AF_INET,
                            .sin_port = htons(socket.port),
                            .sin_addr = {.s_addr = htonl(socket.ip)}};

        // Bind the socket to the address and port
        // and listen for incoming connections
        bind(socket.fd, &addr);
        listen(socket.fd);
    }

    void connect(const TCP_Socket &socket)
    {
        sockaddr_in addr = {.sin_family = AF_INET,
                            .sin_port = htons(socket.port),
                            .sin_addr = {.s_addr = htonl(socket.ip)}};

        auto result = connect(socket.fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));
        if (-1 == result)
        {
            throw my_redis::exception::errno_exception{"void connect(const TCP_Socket& socket)", errno};
        }
    }

    IOResultType read(i32 fd, u8 *buffer, size n)
    {
        while (n > 0)
        {
            ssize bytesRead = ::read(fd, buffer, n);
            if (bytesRead <= 0)
            {
                // If bytesRead is 0, it means EOF has been reached.
                // If bytesRead is -1, an error occurred.
                return bytesRead == 0 ? IOResultType::_EOF : IOResultType::ERR;
            }

            // Assert that the number of bytes read is not greater than the number of bytes to read.
            assert(static_cast<size>(bytesRead) <= n);

            // Decrease the number of bytes to read and move the buffer pointer
            // forward by the number of bytes read.
            n -= static_cast<size>(bytesRead);
            buffer += bytesRead;
        }

        return IOResultType::OK;
    }

    IOResultType write(i32 fd, const u8 *buffer, size n)
    {
        while (n > 0)
        {
            ssize bytesWritten = ::write(fd, buffer, n);
            if (-1 == bytesWritten)
            {
                return IOResultType::ERR;
            }

            assert(static_cast<size>(bytesWritten) <= n);
            n -= static_cast<size>(bytesWritten);
            buffer += bytesWritten;
        }

        return IOResultType::OK;
    }

}  // namespace my_redis::sockets