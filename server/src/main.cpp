#include "exception.hpp"
#include "socket.hpp"
#include "payload.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

namespace
{
    using my_redis::sockets::TCP_Socket;

    // Wait for a client to connect to the server socket
    // and return a new TCP_Socket representing the client connection.
    TCP_Socket waitClient(const TCP_Socket &server)
    {
        sockaddr_in c{};
        socklen_t len = sizeof(c);
        auto fd = accept(server.fd, reinterpret_cast<sockaddr *>(&c), &len);
        if (-1 == fd)
        {
            throw my_redis::exception::errno_exception{"bool waitClient(const TCP_Socket &server, TCP_Socket& client)", errno};
        }
        return {fd, ntohl(c.sin_addr.s_addr), ntohs(c.sin_port)};
    }

    // Handle a client connection by reading the payload from the client socket
    // and printing the message to the console.
    bool handleClient(const TCP_Socket &client)
    {
        using namespace my_redis::payload;

        Payload payload{};
        auto result = readPayload(client.fd, payload);
        if (!result)
        {
            return false;
        }

        auto len = payload.messageLength();
        std::printf("Client[%s] says: %.*s\n", client.toString().c_str(), len, payload.message());

        return true;
    }

}  // namespace

int main(int argc, char *argv[])
{
    using namespace my_redis;

    sockets::TCP_Socket server{INADDR_LOOPBACK, 9999};
    sockets::bindAndListen(server);

    while (true)
    {
        std::printf("> Waiting for a client...\n");
        try
        {
            auto client = waitClient(server);
            while (true)
            {
                auto result = handleClient(client);
                if (!result)
                    break;
            }
        }
        catch (std::exception &ex)
        {
            std::fprintf(stderr, "Error: %s\n", ex.what());
            continue;
        }
    }
}