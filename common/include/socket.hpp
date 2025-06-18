#pragma once

#include "types.hpp"

#include <format>
#include <string>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

namespace my_redis::sockets
{

struct TCP_Socket
{
    i32 fd;
    u32 ip;
    u16 port;

    // Sets ip and port member variables. Creates a file descriptor with socket(), but does not bind or listen yet.
    TCP_Socket(u32 ip, u16 port);

    // Creates a TCP_Socket with an existing file descriptor, IP address, and port.
    constexpr TCP_Socket(i32 fd, u32 ip, u16 port) : fd(fd), ip(ip), port(port)
    {
    }

    // Closes the file descriptor when the TCP_Socket is destroyed.
    ~TCP_Socket();

    TCP_Socket(const TCP_Socket&)               = delete;
    TCP_Socket(TCP_Socket&&)                    = delete;
    TCP_Socket& operator=(const TCP_Socket&)    = delete;
    TCP_Socket operator=(TCP_Socket&&)          = delete;

    // Returns a string representation of the TCP_Socket in the format "ip:port".
    std::string toString() const
    {
        return std::format("{}:{}", inet_ntoa({.s_addr = ip}), port);
    }
};

// Creates a TCP socket, binds it to the specified address and port, and listens for incoming connections.
void bindAndListen(const TCP_Socket& socket);

// Accepts an incoming connection on the specified socket and returns a new TCP_Socket for the accepted connection.
void connect(const TCP_Socket& socket);

enum class IOResultType
{
    OK,
    _EOF,
    ERR
};

IOResultType read(i32 fd, u8* buffer, size n);
IOResultType write(i32 fd, const u8* buffer, size n);

}  // namespace my_redis::sockets