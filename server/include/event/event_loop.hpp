#pragma once

#include "connection.hpp"
#include "event/event_poller.hpp"

#include "socket.hpp"
#include <sys/poll.h>

namespace my_redis
{
    struct Response;

    namespace event
    {
        class EventLoop
        {
        public:
            explicit EventLoop(sockets::ServerSocket &&listener)
            {
                m_EventPoller.addConnection({
                    .type       = EventPoller::ConnectionInfo::Type::LISTENING,
                    .connection = std::make_unique<Connection>(std::move(listener)),
                });
            }

            void run();

        private:
            void dispatch(const pollfd &pfd);
            bool handleAccept(const Connection &connection);
            bool handleRead(Connection &connection);
            bool handleWrite(Connection &connection);

        private:
            EventPoller m_EventPoller;
        };

    } // namespace event
} // namespace my_redis