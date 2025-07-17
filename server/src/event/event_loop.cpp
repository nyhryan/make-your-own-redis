#include "event/event_loop.hpp"

#include "exception.hpp"
#include "hashtable.hpp"
#include "payload.hpp"
#include "response.hpp"
#include "types.hpp"

#include <cassert>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>

#define container_of(ptr, T, member) \
    ((T *)( (char *)ptr - offsetof(T, member) ))

namespace
{
    using namespace my_redis::types;
    using my_redis::hashmap::HashMap;
    using my_redis::hashtable::HashNode;

    struct { HashMap db; } g_data{};

    struct Entry
    {
        HashNode node;
        std::string key, value;
    };

    bool entryCmp(HashNode *lhs, HashNode *rhs) noexcept
    {
        Entry *le = container_of(lhs, Entry, node);
        Entry *re = container_of(rhs, Entry, node);
        return le->key == re->key;
    }
    
    // FNV hash
    constexpr u32 FNV_OFFSET_BASIS  = 0x811C9DC5;
    constexpr u32 FNV_PRIME         = 0x01000193;
    u64 strHash(const u8 *data, size len) noexcept
    {
        u32 h = FNV_OFFSET_BASIS;
        for (size i = 0; i < len; i++)
            h = (h * FNV_PRIME) ^ data[i];
        return h;
    }
    
    static inline u64 strHash(std::string_view str) noexcept
    {
        return strHash((const u8 *) str.data(), str.size());
    }
}

namespace my_redis
{
    constexpr size K_MAX_ARGS = 200 * 1000;

    // Consume 32-bit unsigned integer from the buffer and advance the pointer.
    bool read_u32(const u8 *&curr, const u8 *end, u32 &out)
    {
        if (curr + 4 > end)
            return false;

        std::memcpy(&out, curr, 4);
        curr += 4;
        return true;
    }

    void doGet(std::vector<std::string> &command, Response &out)
    {
        Entry entry{
            .node   = { .hash = strHash(command[1]) },
            .key    = command[1],
            .value  = {}
        };

        // Lookup the entry in the hash map
        HashNode *hashNode = hashmap::lookup(&g_data.db, &entry.node, entryCmp);
        // Not found
        if (!hashNode)
        {
            out.status = Response::Status::RES_NX;
            return;
        }
        // Found
        const std::string &val = container_of(hashNode, Entry, node)->value;
        out.data.assign(val.begin(), val.end());
    }

    void doSet(std::vector<std::string> &command)
    {
        Entry entry{
            .node   = { .hash = strHash(command[1]) },
            .key    = command[1],
            .value  = {}
        };

        // Lookup the entry in the hash map
        HashNode *hashNode = hashmap::lookup(&g_data.db, &entry.node, entryCmp);
        // If found, update the value
        if (hashNode)
        {
            container_of(hashNode, Entry, node)->value.swap(command[2]);
            return;
        }
     
        // If not found, create a new entry
        Entry *newEntry = new Entry{};
        newEntry->node.hash = entry.node.hash;
        newEntry->key.swap(entry.key);
        newEntry->value.swap(command[2]);
        hashmap::insert(&g_data.db, &newEntry->node);
    }

    void doDel(std::vector<std::string> &command)
    {
        Entry entry{
            .node   = { .hash = strHash(command[1]) },
            .key    = command[1],
            .value  = {}
        };

        // Lookup and detach the entry in the hash map
        HashNode *removedHashNode = hashmap::remove(&g_data.db, &entry.node, entryCmp);
        // If found, delete the entry
        if (removedHashNode)
            delete container_of(removedHashNode, Entry, node);
    }

    std::optional<std::vector<std::string>> parseRequest(const u8 *data, size size)
    {
        // Request format: "nstr(count of strings) | len1     str1         | len2 str2 | ... | lenN strN"
        //                  ^ 4Bytes                 ^ 4Bytes ^ len1 Bytes

        const u8 *curr = data;
        const u8 *end = data + size;
        std::vector<std::string> command;

        u32 nstr = 0;
        if (!read_u32(curr, end, nstr))
            return { std::nullopt };

        if (nstr > K_MAX_ARGS)
            return { std::nullopt };

        // Read command
        while (command.size() < nstr)
        {
            // Read the length of the next string
            u32 len = 0;
            if (!read_u32(curr, end, len))
                return { std::nullopt };

            if (curr + len > end)
                return { std::nullopt };

            // Read the string itself
            command.emplace_back(std::string{curr, curr + len});
            curr += len;
        }

        if (curr != end)
            return { std::nullopt };

        return { command };
    }

    Response handleRequest(std::vector<std::string> &command)
    {
        Response response{};
        if (command.size() == 2 && command.at(0) == "get")
            doGet(command, response);
        else if (command.size() == 3 && command.at(0) == "set")
            doSet(command);
        else if (command.size() == 2 && command.at(0) == "del")
            doDel(command);
        else
            response.status = Response::Status::RES_ERR;

        return response;
    }

    bool tryParseRequest(Connection &connection)
    {
        // Not enough data to read the header
        if (connection.incomingBuffer.size() < payload::HEADER_LEN)
            return false;

        // 1. Read the length of the payload
        u32 requestLen = 0;
        std::memcpy(&requestLen, connection.incomingBuffer.data(), payload::HEADER_LEN);
        if (requestLen > payload::MAX_MSG_LEN)
        {
            std::fprintf(stderr, "> Requested message is too long!\n");
            connection.wantClose = true;
            return false;
        }

        // Not enough data to read the entire payload
        if (connection.incomingBuffer.size() < payload::HEADER_LEN + requestLen)
            return false;

        // Request ready to be processed
        const u8 *request = connection.incomingBuffer.data() + payload::HEADER_LEN;

        std::optional<std::vector<std::string>> command = parseRequest(request, requestLen);
        if (!command)
        {
            std::fprintf(stderr, "> Bad Request\n");
            connection.wantClose = true;
            return false;
        }

        {
            std::stringstream ss;
            for (auto i = 0; i < command->size() - 1; ++i)
                ss << command->at(i) << ", ";
            ss << command->back();
            std::fprintf(stderr, "> Parsed Request: [ %s ]\n", ss.str().c_str());
        }

        Response resp = handleRequest(*command);

        // Prepare the response
        u32 responseLength = resp.data.size() + 4;
        buffer::append(connection.outgoingBuffer, &responseLength, 4);
        buffer::append(connection.outgoingBuffer, &resp.status, 4);
        buffer::append(connection.outgoingBuffer, resp.data.data(), resp.data.size());

        // Consume the processed payload
        buffer::consume(connection.incomingBuffer, payload::HEADER_LEN + requestLen);

        return true;
    }
}

namespace my_redis::event
{
    void EventLoop::run()
    {
        while (true)
        {
            if (!m_EventPoller.poll())
                continue;

            // Dispatch events for all ready connections
            for (const auto &pfd : m_EventPoller.ready())
                dispatch(pfd);
        }
    }

    void EventLoop::dispatch(const pollfd &pfd)
    {
        auto &[fd, events, revents] = pfd;
        auto &[type, connection] = m_EventPoller.connections().at(fd);

        if (type == EventPoller::ConnectionInfo::Type::LISTENING)
        {
            // handle accept
            if (!handleAccept(*connection))
                std::fprintf(stderr, "> Failed to accept new client!\n");

            return;
        }

        // handle read
        if (revents & POLLIN)
            handleRead(*connection);
        
        // handle write
        if (revents & POLLOUT)
            handleWrite(*connection);

        // handle error and close
        if (revents & POLLERR || connection->wantClose)
        {
            std::fprintf(stderr, "> Client disconnected.\n");
            m_EventPoller.closeConnection(connection->fd());
        }
    }

    bool EventLoop::handleAccept(const Connection &connection)
    {
        sockaddr_in addr{};
        socklen_t len{ sizeof(addr) };

        auto fd = ::accept(connection.fd(), (struct sockaddr *)&addr, &len);
        if (fd >= 0)
        {
            std::fprintf(stderr, "> Accepted new client[%s:%d]\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

            sockets::Socket client{ fd };
            client.setNonBlock();
            auto clientConnection = std::make_unique<Connection>(std::move(client));
            clientConnection->wantRead = true;

            m_EventPoller.addConnection(EventPoller::ConnectionInfo{
                .type       = EventPoller::ConnectionInfo::Type::CLIENT,
                .connection = std::move(clientConnection),
            });

            return true;
        }

        if (errno == EINTR || errno == EAGAIN)
            return false;

        if (-1 == fd)
            throw exception::errno_exception{ "bool handleAccept(const Connection &connection) -> accept()" };

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
                std::fprintf(stderr, "bool handleRead(ConnectionImpl &connection) -> read() : %s\n", util::strerror(errno).c_str());
                connection.wantClose = true;
            }
            return false;
        }

        if (0 == bytesRead) // EOF
        {
            if (connection.incomingBuffer.size() != 0)
                std::fprintf(stderr, "> Unexpected EOF(read %zd bytes - incomingBuffer size %zu)\n", bytesRead, connection.incomingBuffer.size());

            connection.wantClose = true;
            return false;
        }

        // Successfully read data
        buffer::append(connection.incomingBuffer, buffer, bytesRead);

        // Pipeline processing of requests
        // Keep processing until no more complete requests are available
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
                std::fprintf(stderr, "bool handleWrite(Connection &connection) -> write() : %s\n", util::strerror(errno).c_str());
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
} // namespace my_redis::event