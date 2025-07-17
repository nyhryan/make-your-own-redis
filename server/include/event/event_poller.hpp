#pragma once

#include "connection.hpp"
#include <memory>
#include <ranges>

namespace my_redis::event
{
    class EventPoller
    {
    public:
        struct ConnectionInfo
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
        void addConnection(ConnectionInfo &&connectionInfo);

    public:
        // Returns a view of the connections that are ready for reading or writing.
        constexpr auto ready() const noexcept
        {
            return m_Pfds 
                | std::views::filter([](const pollfd &pfd) { return pfd.revents; });
        }

        constexpr auto &connections() noexcept { return m_Connections; }

        void closeConnection(types::i32 fd)
        {
            m_Connections.at(fd).connection.reset(nullptr);
        }

    private:
        std::vector<pollfd> m_Pfds;
        std::vector<ConnectionInfo> m_Connections;
    };
} // namespace my_redis::event