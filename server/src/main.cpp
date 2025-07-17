#include "event/event_loop.hpp"
#include "socket.hpp"

int main(int argc, char *argv[])
{
    using namespace my_redis;

    sockets::Endpoint ep{"127.0.0.1", 9999};
    sockets::ServerSocket server{ep};

    event::EventLoop eventLoop{std::move(server)};
    eventLoop.run();
}