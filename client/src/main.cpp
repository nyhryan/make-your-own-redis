#include "socket.hpp"
#include "response.hpp"

#include <cstdio>

#include <cstring>
#include <string>
#include <vector>

namespace
{
    using namespace my_redis;
    using namespace my_redis::types;
    using command = std::vector<std::string>;

    constexpr size K_MAX_MSG = 4096;

    i32 sendRequest(const sockets::Socket &server, const command &command)
    {
        u32 len = 4;
        for (const auto &s: command)
            len += 4 + s.size();

        if (len > K_MAX_MSG)
            return -1;
        
        u8 wbuf[4 + K_MAX_MSG];
        std::memcpy(wbuf, &len, 4);

        u32 n = command.size();
        std::memcpy(wbuf + 4, &n, 4);

        size curr = 8;
        for (const auto &s: command)
        {
            u32 commandSize = s.size();
            // Length of the command
            std::memcpy(wbuf + curr, &commandSize, 4);
            curr += 4;

            // The command itself
            std::memcpy(wbuf + curr, s.data(), commandSize);
            curr += commandSize;
        }
        
        sockets::write(server.fd(), wbuf, 4 + len);
        return 0;
    }

    i32 readResponse(const sockets::Socket &server)
    {
        u8 rbuf[4 + K_MAX_MSG + 1];
        u32 len = 0;
        sockets::read(server.fd(), rbuf, 4);
        std::memcpy(&len, rbuf, 4);

        sockets::read(server.fd(), rbuf + 4, len);
        
        Response::Status status;
        std::memcpy(&status, rbuf + 4, 4);
        std::printf("Server says : [%s] %.*s\n", statusStr(status), len - 4, rbuf + 8);
        return 0;
    }

} // namespace

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::fprintf(stderr, "Usage:\n"
                             "  %s <command> [<arg1> <arg2> ...]\n"
                             "Example:\n"
                             "  %s set key value\n"
                             "  %s get key\n"
                             "  %s del key\n", argv[0], argv[0], argv[0], argv[0]);

        return 1;
    }

    using namespace my_redis;
    sockets::Endpoint ep{ "127.0.0.1", 9999 };
    sockets::Socket server{ ::socket(AF_INET, SOCK_STREAM, 0) };

    auto sockaddr = ep.sockaddr();
    if (-1 == ::connect(server.fd(), (const struct sockaddr *)&sockaddr, ep.socklen()))
    {
        std::perror("connect()");
        return 1;
    }

    command cmd{ argv + 1, argv + argc };
    sendRequest(server, cmd);
    readResponse(server);
}