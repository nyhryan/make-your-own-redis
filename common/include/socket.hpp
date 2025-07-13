#pragma once

#include "types.hpp"

#include <fcntl.h>
#include <netinet/ip.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <unistd.h>

#include <format>
#include <string>
#include <utility>

namespace my_redis::sockets
{
    struct Endpoint
    {
        const char *ip;
        types::u16 port;

        constexpr sockaddr_in sockaddr() const noexcept
        {
            return sockaddr_in{
                .sin_family = AF_INET,
                .sin_port = htons(port),
                .sin_addr = { .s_addr = inet_addr(ip) },
            };
        }

        constexpr socklen_t socklen() const noexcept { return sizeof(sockaddr()); }

        constexpr std::string toString() const noexcept
        {
            return std::format("{}:{}", ip, ntohs(port));
        }
    };

    class Socket
    {
    public:
        Socket() = default;
        explicit Socket(types::i32 fd) : m_Fd(fd) {}
        virtual ~Socket()
        {
            if (m_Fd >= 0)
            {
                ::close(m_Fd);
            }
        }

        Socket(Socket &&other) : m_Fd(std::exchange(other.m_Fd, -1))
        {
        }
        
        Socket &operator=(Socket &&other) noexcept
        {
            if (this != &other)
            {
                if (m_Fd >= 0)
                    ::close(m_Fd);

                m_Fd = std::exchange(other.m_Fd, -1);
            }

            return *this;
        }

        Socket(const Socket &)              = delete;
        Socket &operator=(const Socket &)   = delete;

    public:
        void setNonBlock() const;
        constexpr types::i32 fd() const noexcept { return m_Fd; }
        constexpr bool isValid() const noexcept { return m_Fd >= 0; }

    private:
        types::i32 m_Fd{ -1 };
    };

    class ServerSocket : public my_redis::sockets::Socket
    {
    public:
        using Socket::operator=;
        ServerSocket(const Endpoint &endpoint);

    public:
        Socket accept() const;
    };

    enum class IOResultType
    {
        OK,
        _EOF,
        ERR
    };

    IOResultType read(types::i32 fd, types::u8 *buffer, types::size n);
    IOResultType write(types::i32 fd, const types::u8 *buffer, types::size n);

} // namespace my_redis::sockets