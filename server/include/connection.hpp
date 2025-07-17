#pragma once

#include "buffer.hpp"
#include "socket.hpp"

namespace my_redis
{
    struct Connection
    {
        explicit Connection(sockets::Socket socket) noexcept : socket(std::move(socket)) {}
        ~Connection() = default;

        Connection(Connection &&)               = default;
        Connection &operator=(Connection &&)    = default;

        Connection(const Connection &)              = delete;
        Connection &operator=(const Connection &)   = delete;

        types::i32 fd() const noexcept { return socket.fd(); }
        bool isValid() const noexcept { return socket.isValid(); }

        sockets::Socket socket;
        buffer::buffer_t incomingBuffer;
        buffer::buffer_t outgoingBuffer;
        bool wantRead{ false };
        bool wantWrite{ false };
        bool wantClose{ false };
    };
} // namespace my_redis::event