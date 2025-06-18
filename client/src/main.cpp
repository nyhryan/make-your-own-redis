#include "socket.hpp"
#include "payload.hpp"

#include <netinet/ip.h>

namespace
{
    using namespace my_redis;

    // Function to send a query to the server
    bool query(const sockets::TCP_Socket &socket, const char *text)
    {
        payload::Payload req{text};
        return payload::writePayload(socket.fd, req);
    }

}  // namespace

int main(int argc, char *argv[])
{
    using namespace my_redis;

    // Connect server at localhost:9999
    sockets::TCP_Socket server{INADDR_LOOPBACK, 9999};
    sockets::connect(server);

    // Send a couple of messages to the server
    if (!::query(server, "Hello 1"))
        return 0;

    if (!::query(server, "Hello 2"))
        return 0;
}