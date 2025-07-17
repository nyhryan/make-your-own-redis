#include "event/event_poller.hpp"
#include "exception.hpp"

#include <ranges>

namespace my_redis::event
{
    bool EventPoller::poll()
    {
        m_Pfds.clear();

        // Prepare pollfd structures for all connections
        // Filter out empty connections
        auto filtered = m_Connections |
                        std::views::filter([](const ConnectionInfo &ci)
                                           { return ci.connection != nullptr; });

        m_Pfds.reserve(std::ranges::distance(filtered));
        
        // Fill pollfd structures
        for (const auto &[type, connection] : filtered)
        {
            m_Pfds.emplace_back(pollfd{
                .fd = connection->fd(),
                .events = POLLERR, // Listen for errors by default
                .revents = 0
            });
            m_Pfds.back().events |= (type == ConnectionInfo::Type::LISTENING ? POLLIN : 0) |
                                    (connection->wantRead ? POLLIN : 0) |
                                    (connection->wantWrite ? POLLOUT : 0);
        }

        auto result = ::poll(m_Pfds.data(), m_Pfds.size(), -1);
        if (-1 == result)
        {
            if (errno == EINTR)
                return false;
            else
                throw exception::errno_exception{ "bool EventPoller::poll() -> poll()" };
        }

        return true;
    }

    void EventPoller::addConnection(ConnectionInfo &&connectionInfo)
    {
        auto fd = connectionInfo.connection->fd();
        if (m_Connections.size() < fd + 1)
            m_Connections.resize(fd + 1);

        m_Connections.at(fd) = std::move(connectionInfo);
    }
}