#include "payload.hpp"
#include "socket.hpp"
#include "util.hpp"

#include <cstdio>
#include <cassert>

#include <stdexcept>

namespace my_redis::payload
{

    Payload::Payload(std::string_view message)
    {
        if (message.length() > MSG_LEN)
        {
            throw std::runtime_error{"Message of the request is too long!"};
        }

        size len = message.length();
        // 1. Set the header with the length of the message
        std::memcpy(this->m_Buffer, &len, HEADER_LEN);

        // 2. Copy the message into the buffer after the header
        std::memcpy(this->m_Buffer + HEADER_LEN, message.data(), len);

        // Assert that the length in the header matches the message length
        len = 0;
        std::memcpy(&len, this->m_Buffer, HEADER_LEN);
        assert(len == message.length());
    }

    bool readPayload(i32 fd, Payload &request)
    {
        using namespace my_redis::sockets;

        // 1. Read the header first
        auto result = read(fd, request.m_Buffer, HEADER_LEN);
        switch (result)
        {
            case IOResultType::OK:
                break;

            case IOResultType::_EOF:
            {
                std::fprintf(stderr, "EOF\n");
                return false;
            }
            break;

            case IOResultType::ERR:
            default:
            {
                std::fprintf(stderr, "Error: %s\n", my_redis::util::strerror(errno).c_str());
                return false;
            }
            break;
        }

        auto length = request.messageLength();
        if (length > MSG_LEN)
        {
            return false;
        }

        // 2. Read the payload based on the length specified in the header
        result = read(fd, request.m_Buffer + HEADER_LEN, length);
        return result == IOResultType::OK;
    }

    bool writePayload(i32 fd, Payload &payload)
    {
        using namespace my_redis::sockets;

        auto len = payload.messageLength();
        auto result = write(fd, payload.m_Buffer, HEADER_LEN + len);
        return result == IOResultType::OK;
    }

}  // namespace my_redis::payload
