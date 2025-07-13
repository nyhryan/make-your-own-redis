#include "socket.hpp"
#include "payload.hpp"

#include <cstdio>

#include <string_view>
#include <string>
#include <vector>

namespace
{
    using namespace my_redis;
    using namespace my_redis::types;

    constexpr size K_MAX_MSG = 4096;

    bool writeRequest(const sockets::Socket &server, std::string_view msg)
    {
        if (!payload::writePayload(server.fd(), { msg }))
        {
            std::perror("> Something went wrong");
            return false;
        }
        return true;
    }

    void readResponse(const sockets::Socket &socket)
    {
        payload::Payload response{};
        payload::readPayload(socket.fd(), response);
        auto len = response.messageLength();
        auto precision = len > 100 ? 100 : len;
        std::printf("response: %.*s\n\n", precision, response.message());
    }

    i32 sendRequest(const sockets::Socket &server, const std::vector<std::string> &commands)
    {
        u32 len = 4;
        for (const auto &s: commands)
            len += 4 + s.size();

        if (len > K_MAX_MSG)
            return -1;
        
        u8 wbuf[4 + K_MAX_MSG];
        std::memcpy(&wbuf[0], &len, 4);
        u32 n = commands.size();
        std::memcpy(&wbuf[4], &n, 4);
        size curr = 8;
        for (const auto &s: commands)
        {
            u32 p = s.size();
            std::memcpy(&wbuf[curr], &p, 4);
            std::memcpy(&wbuf[curr + 4], s.data(), s.size());
            curr += 4 + s.size();
        }
        
        sockets::write(server.fd(), wbuf, 4 + len);
        return 0;
    }

    i32 readResponse2(const sockets::Socket &server)
    {
        u8 rbuf[4 + K_MAX_MSG + 1];
        errno = 0;
        sockets::read(server.fd(), rbuf, 4);
        
        u32 len = 0;
        std::memcpy(&len, rbuf, 4);
        
        sockets::read(server.fd(), rbuf + 4, len);
        u32 responseCode = 0;
        std::memcpy(&responseCode, rbuf + 4, 4);
        std::printf("Server says : [%u] %.*s\n", responseCode, len - 4, rbuf + 8);
        return 0;
    }

} // namespace

/* int main(int argc, char *argv[])
{
    using namespace my_redis;
    sockets::Endpoint ep{"127.0.0.1", 9999};
    sockets::Socket server{ ::socket(AF_INET, SOCK_STREAM, 0) };

    auto sockaddr = ep.sockaddr();
    if (-1 == ::connect(server.fd(), (const struct sockaddr*) &sockaddr, ep.socklen()))
    {
        std::perror("connect()");
        return 1;
    }

    std::array<std::string, 5> messages =
    {
        "Hello 1",
        "Hello 2",
        "Hello 3!",
        std::string(payload::MSG_LEN, 'z'),
        "Hello 5"
    };

    for (const auto &msg : messages)
    {
        if (!writeRequest(server, msg))
            return 1;
    }

    for (auto i = 0; i < messages.size(); ++i)
    {
        readResponse(server);
    }
} */

int main(int argc, char *argv[])
{
    using namespace my_redis;
    sockets::Endpoint ep{ "127.0.0.1", 9999 };
    sockets::Socket server{ ::socket(AF_INET, SOCK_STREAM, 0) };

    auto sockaddr = ep.sockaddr();
    if (-1 == ::connect(server.fd(), (const struct sockaddr *)&sockaddr, ep.socklen()))
    {
        std::perror("connect()");
        return 1;
    }

    std::vector<std::string> commands;
    for (auto i = 1; i < argc; ++i)
        commands.push_back(argv[i]);
 
    sendRequest(server, commands);
    readResponse2(server);
}