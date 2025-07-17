#include "payload.hpp"
#include "socket.hpp"
#include "util.hpp"

#include <cstdio>
#include <cassert>

#include <stdexcept>

namespace my_redis::payload
{
    using namespace my_redis::types;
    
    Payload::Payload(std::string_view message)
    {
        if (message.length() > MAX_MSG_LEN)
        {
            throw std::runtime_error{"Message of the request is too long!"};
        }

        size len = message.length();
        // 1. Set the header with the length of the message
        buffer::append(m_Buffer, &len, HEADER_LEN);
        // 2. Copy the message into the buffer after the header
        buffer::append(m_Buffer, message.data(), len);
        
        // Assert that the length in the header matches the message length
        len = 0;
        std::memcpy(&len, this->m_Buffer.data(), HEADER_LEN);
        assert(len == message.length());
    }

    bool readPayload(i32 fd, Payload &request)
    {
        // 1. Read the header first
        request.m_Buffer.resize(HEADER_LEN);
        auto result = sockets::read(fd, request.m_Buffer.data(), HEADER_LEN);
        switch (result)
        {
            case sockets::IOResultType::OK:
                break;

            case sockets::IOResultType::_EOF:
            {
                std::fprintf(stderr, "EOF\n");
                return false;
            }
            break;

            case sockets::IOResultType::ERR:
            default:
            {
                std::fprintf(stderr, "bool readPayload(i32 fd, Payload &request) -> read(): %s\n", my_redis::util::strerror(errno).c_str());
                return false;
            }
            break;
        }

        auto length = request.messageLength();
        if (length > MAX_MSG_LEN)
        {
            return false;
        }

        // 2. Read the payload based on the length specified in the header
        request.m_Buffer.resize(HEADER_LEN + length);
        result = sockets::read(fd, request.m_Buffer.data() + HEADER_LEN, length);
        return result == sockets::IOResultType::OK;
    }

    bool writePayload(i32 fd, Payload &&payload)
    {
        auto len = payload.messageLength();
        auto result = sockets::write(fd, payload.m_Buffer.data(), HEADER_LEN + len);
        return result == sockets::IOResultType::OK;
    }

}  // namespace my_redis::payload
