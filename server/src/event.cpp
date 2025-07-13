#include "event.hpp"
#include "buffer.hpp"
#include "exception.hpp"
#include "payload.hpp"
#include "socket.hpp"
#include "util.hpp"

#include <cassert>
#include <map>
#include <unistd.h>
#include <memory>

namespace
{
    std::map<std::string, std::string> g_data{};
}

namespace my_redis::event
{
    using namespace my_redis::types;

    constexpr size K_MAX_ARGS = 200 * 1000;

    bool EventPoller::poll()
    {
        m_Pfds.clear();

        for (const auto &[type, connection] : m_Connections)
        {
            if (!connection)
                continue;

            pollfd pfd{
                .fd = connection->fd(),
                .events = 0,
                .revents = 0,
            };

            pfd.events = POLLERR // Want to recieve Error events by default;
                         | (type == ConnectionMeta::Type::LISTENING ? POLLIN : 0) | (connection->wantRead ? POLLIN : 0) | (connection->wantWrite ? POLLOUT : 0);

            m_Pfds.emplace_back(pfd);
        }

        auto result = ::poll(m_Pfds.data(), m_Pfds.size(), -1);
        if (-1 == result)
        {
            if (errno == EINTR)
                return false;
            else
                throw exception::errno_exception{ "void EventLoop::run() -> poll()" };
        }

        return true;
    }

    void EventPoller::addConnectionMeta(ConnectionMeta &&connection)
    {
        auto fd = connection.connection->fd();
        if (m_Connections.size() < fd + 1)
            m_Connections.resize(fd + 1);

        m_Connections.at(fd) = std::move(connection);
    }

    void EventLoop::run()
    {
        while (true)
        {
            if (!m_EventPoller.poll())
                continue;

            for (const auto &pfd : m_EventPoller.ready())
            {
                dispatch(pfd);
            }
        }
    }

    void EventLoop::dispatch(const pollfd &pfd)
    {
        auto &[fd, events, revents] = pfd;
        auto &[type, ptr_Connection] = m_EventPoller.connections().at(fd);

        if (type == EventPoller::ConnectionMeta::Type::LISTENING)
        {
            // handle accept
            if (!handleAccept(*ptr_Connection))
                std::fprintf(stderr, "> Failed to accept new client!\n");

            return;
        }

        if (revents & POLLIN)
        {
            // handle read
            handleRead(*ptr_Connection);
        }

        if (revents & POLLOUT)
        {
            // handle write
            handleWrite(*ptr_Connection);
        }

        if (revents & POLLERR || ptr_Connection->wantClose)
        {
            // handle error and close
            std::fprintf(stderr, "> Client disconnected.\n");

            auto fd = ptr_Connection->fd();
            m_EventPoller.connections()[fd].connection.reset();
            m_EventPoller.connections()[fd] = {};
        }
    }

    bool EventLoop::handleAccept(const Connection &connection)
    {
        sockaddr_in addr{};
        socklen_t len{ sizeof(addr) };

        auto fd = ::accept(connection.fd(), (struct sockaddr *)&addr, &len);
        if (fd >= 0)
        {
            std::fprintf(stderr, "> new client! [%s:%d]\n", inet_ntoa(addr.sin_addr), addr.sin_port);

            sockets::Socket client{ fd };
            client.setNonBlock();
            auto clientConnection = std::make_unique<Connection>(std::move(client));
            clientConnection->wantRead = true;

            m_EventPoller.addConnectionMeta({
                .type = EventPoller::ConnectionMeta::Type::CLIENT,
                .connection = std::move(clientConnection),
            });
            return true;
        }

        if (errno == EINTR || errno == EAGAIN)
            return false;

        if (-1 == fd)
            throw exception::errno_exception{ "bool EventLoop::handleAccept(const sockets::Socket &socket) -> accept()" };

        return true;
    }

    bool EventLoop::handleRead(Connection &connection)
    {
        u8 buffer[64 * 1024];
        ssize bytesRead = ::read(connection.fd(), buffer, sizeof(buffer));
        if (-1 == bytesRead) // Error
        {
            if (errno != EAGAIN)
            {
                std::fprintf(stderr, "bool EventLoop::handleRead(ConnectionImpl &connection) -> read() : %s\n", my_redis::util::strerror(errno).c_str());
                connection.wantClose = true;
            }
            return false;
        }

        if (0 == bytesRead) // EOF
        {
            if (connection.incomingBuffer.size() != 0)
                std::fprintf(stderr, "> Unexpected EOF(read %zd bytes - incomingBuffer size %zu)\n", bytesRead, connection.incomingBuffer.size());
            // else
            // std::fprintf(stderr, "> Client closed\n");

            connection.wantClose = true;
            return false;
        }

        buffer::append(connection.incomingBuffer, buffer, bytesRead);

        while (tryParseRequest(connection))
            ;

        if (connection.outgoingBuffer.size() > 0)
        {
            connection.wantRead = false;
            connection.wantWrite = true;
            return handleWrite(connection);
        }

        return true;
    }

    bool EventLoop::handleWrite(Connection &connection)
    {
        assert(connection.outgoingBuffer.size() > 0);

        ssize bytesWritten = ::write(connection.fd(), connection.outgoingBuffer.data(), connection.outgoingBuffer.size());
        if (-1 == bytesWritten)
        {
            if (errno != EAGAIN)
            {
                std::fprintf(stderr, "bool EventLoop::handleWrite(ConnectionImpl &connection) -> write() : %s\n", util::strerror(errno).c_str());
                connection.wantClose = true;
            }
            return false;
        }

        buffer::consume(connection.outgoingBuffer, bytesWritten);

        if (connection.outgoingBuffer.size() == 0)
        {
            connection.wantRead = true;
            connection.wantWrite = false;
        }

        return true;
    }

    bool EventLoop::tryParseRequest(Connection &connection)
    {
        if (connection.incomingBuffer.size() < payload::HEADER_LEN)
            return false;

        u32 len = 0;
        std::memcpy(&len, connection.incomingBuffer.data(), payload::HEADER_LEN);
        if (len > payload::MSG_LEN)
        {
            std::fprintf(stderr, "> Requested message is too long!\n");
            connection.wantClose = true;
            return false;
        }

        if (connection.incomingBuffer.size() < payload::HEADER_LEN + len)
            return false;

        const u8 *payload = connection.incomingBuffer.data() + payload::HEADER_LEN;

        std::vector<std::string> commands;
        if (parseRequest(payload, len, commands) < 0)
        {
            fprintf(stderr, "> Bad Request\n");
            connection.wantClose = true;
            return false;
        }

        Response resp;
        doRequest(commands, resp);
        makeResponse(resp, connection.outgoingBuffer);

        // auto precision = len < 100 ? len : 100;
        // std::fprintf(stderr, "> client says: %.*s\n", precision, payload);

        // buffer::append(connection.outgoingBuffer, &len, payload::HEADER_LEN);
        // buffer::append(connection.outgoingBuffer, payload, len);
        // std::fprintf(stderr, "> replying '%.*s' with legnth %u to client\n", len, payload, len);

        buffer::consume(connection.incomingBuffer, payload::HEADER_LEN + len);

        return true;
    }

    constexpr bool read_u32(const u8 *&curr, const u8 *end, u32 &out)
    {
        if (curr + 4 > end)
            return false;

        std::memcpy(&out, curr, 4);
        curr += 4;
        return true;
    }

    constexpr bool read_str(const u8 *&curr, const u8 *end, size n, std::string &out)
    {
        if (curr + n > end)
            return false;

        out.assign(curr, curr + n);
        curr += n;
        return true;
    }

    i32 EventLoop::parseRequest(const u8 *data, size size, std::vector<std::string> &commands)
    {
        const u8 *end = data + size;
        u32 nstr = 0;
        if (!read_u32(data, end, nstr))
            return -1;
        if (nstr > K_MAX_ARGS)
            return -1;

        while (commands.size() < nstr)
        {
            u32 len = 0;
            if (!read_u32(data, end, len))
                return -1;

            commands.emplace_back("");
            if (!read_str(data, end, len, commands.back()))
                return -1;
        }

        if (data != end)
            return -1;

        return 0;
    }

    void EventLoop::doRequest(std::vector<std::string> &commands, Response &out)
    {
        if (commands.size() == 2 && commands.at(0) == "get")
        {
            auto it = ::g_data.find(commands.at(1));
            if (it == ::g_data.end())
            {
                out.status = Response::Status::RES_NX;
                return;
            }

            const std::string &value = it->second;
            out.data.assign(value.begin(), value.end());
        }
        else if (commands.size() == 3 && commands.at(0) == "set")
            ::g_data[commands.at(1)].swap(commands.at(2));
        else if (commands.size() == 2 && commands.at(0) == "del")
            ::g_data.erase(commands.at(1));
        else
            out.status = Response::Status::RES_ERR;
    }

    void EventLoop::makeResponse(const Response &resp, buffer::buffer_t &out)
    {
        u32 responseLength = 4 + static_cast<u32>(resp.data.size());
        buffer::append(out, &responseLength, 4);
        buffer::append(out, &resp.status, 4);
        buffer::append(out, resp.data.data(), resp.data.size());
    }


} // namespace my_redis::event