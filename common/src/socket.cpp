#include "socket.hpp"
#include "exception.hpp"
#include "types.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cassert>

namespace my_redis::sockets
{
    using namespace my_redis::types;

    void Socket::setNonBlock() const
    {
        auto flags = ::fcntl(m_Fd, F_GETFL);
        if (-1 == flags)
        {
            throw exception::errno_exception{ "void Socket::setNonBlock() const -> fcntl(F_GETFL)" };
        }

        flags |= O_NONBLOCK;
        
        if (-1 == ::fcntl(m_Fd, F_SETFL, flags))
        {
            throw exception::errno_exception{ "void Socket::setNonBlock() const -> fcntl(F_SETFL)" };
        }        
    }

    ServerSocket::ServerSocket(const Endpoint &endpoint)
    {
        Socket s{ ::socket(AF_INET, SOCK_STREAM, 0) };
        if (!s.isValid())
        {
            throw exception::errno_exception{ "ServerSocket::ServerSocket(const Endpoint &endpoint) -> socket()" };
        }

        auto _true = 1;
        setsockopt(s.fd(), SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(_true));

        s.setNonBlock();

        auto sockaddr = endpoint.sockaddr();
        if (-1 == ::bind(s.fd(), (const struct sockaddr *)&sockaddr, sizeof(sockaddr)))
        {
            throw exception::errno_exception{ "ServerSocket::ServerSocket(const Endpoint &endpoint) -> bind()" };
        }
        
        if (-1 == ::listen(s.fd(), SOMAXCONN))
        {
            throw exception::errno_exception{ "ServerSocket::ServerSocket(const Endpoint &endpoint) -> listen()" };
        }

        this->operator=(std::move(s));
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

} // namespace my_redis::sockets