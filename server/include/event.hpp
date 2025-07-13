#pragma once

#include "buffer.hpp"
#include "socket.hpp"
#include "types.hpp"
#include <sys/poll.h>
#include <memory>
#include <vector>
#include <ranges>

namespace my_redis::event
{
    struct Connection
    {
        explicit Connection(sockets::Socket socket) noexcept : socket(std::move(socket)) {}
        ~Connection() = default;

        Connection(Connection &&) = default;
        Connection &operator=(Connection &&) = default;
        Connection(const Connection &) = delete;
        Connection &operator=(const Connection &) = delete;

        constexpr types::i32 fd() const noexcept { return socket.fd(); }
        constexpr bool isValid() const noexcept { return socket.isValid(); }

        sockets::Socket socket;
        buffer::buffer_t incomingBuffer;
        buffer::buffer_t outgoingBuffer;
        bool wantRead{ false };
        bool wantWrite{ false };
        bool wantClose{ false };
    };

    struct Response
    {
        enum class Status
        {
            RES_OK = 0,
            RES_ERR,
            RES_NX
        } status = Status::RES_OK;
        std::vector<types::u8> data;
    };

    class EventPoller
    {
    public:
        struct ConnectionMeta
        {
            enum class Type
            {
                LISTENING,
                CLIENT
            } type;
            std::unique_ptr<Connection> connection;
        };

    public:
        bool poll();
        void addConnectionMeta(ConnectionMeta &&connection);

    public:
        constexpr auto ready() const noexcept
        {
            return m_Pfds | std::views::filter([](pollfd pfd)
                                               { return pfd.revents; });
        }
        constexpr auto &connections() noexcept { return m_Connections; }

    private:
        std::vector<pollfd> m_Pfds;
        std::vector<ConnectionMeta> m_Connections;
    };

    class EventLoop
    {
    public:
        explicit EventLoop(sockets::ServerSocket &&listener)
        {
            m_EventPoller.addConnectionMeta({
                .type = EventPoller::ConnectionMeta::Type::LISTENING,
                .connection = std::make_unique<Connection>(std::move(listener)),
            });
        }

        void run();

    private:
        void dispatch(const pollfd &pfd);
        bool handleAccept(const Connection &connection);
        bool handleRead(Connection &connection);
        bool handleWrite(Connection &connection);
        bool tryParseRequest(Connection &connection);
        types::i32 parseRequest(const types::u8 *data, types::size size, std::vector<std::string> &commands);
        void doRequest(std::vector<std::string> &commands, Response &out);
        void makeResponse(const Response &resp, buffer::buffer_t &out);

    private:
        EventPoller m_EventPoller;
    };

} // namespace my_redis::event