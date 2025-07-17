#pragma once

#include "buffer.hpp"
#include "types.hpp"

#include <cstring>

#include <string_view>

namespace my_redis::payload
{
    /*
        * Payload structure:
        * | Header (4 bytes) | Message |
        * The header contains the length of the message in bytes.
        * The message is a null-terminated string.
    */
    constexpr types::size HEADER_LEN = 4;
    // constexpr size MSG_LEN = 4096;
    constexpr types::size MAX_MSG_LEN = 32 << 20;
    constexpr types::size PAYLOAD_LEN = HEADER_LEN + MAX_MSG_LEN + 1;

    class Payload
    {
        friend bool readPayload(types::i32 fd, Payload &payload);
        friend bool writePayload(types::i32 fd, Payload &&payload);

    public:
        Payload() = default;

        // Used to create a payload from a string message(sending payload)
        Payload(std::string_view message);

        Payload(const Payload &) = delete;
        Payload(Payload &&) = delete;
        Payload &operator=(const Payload &) = delete;
        Payload operator=(Payload &&) = delete;

    public:
        // Get the length of the message
        types::u32 messageLength()
        {
            types::u32 length = 0;
            std::memcpy(&length, m_Buffer.data(), HEADER_LEN);
            return length;
        }

        // Get the start address of the message
        constexpr const types::u8 *message() { return m_Buffer.data() + HEADER_LEN; }

    private:
        // Raw buffer to hold the payload
        buffer::buffer_t m_Buffer;        
    };

    // Read a payload from the given file descriptor
    bool readPayload(types::i32 fd, Payload &payload);

    // Write a payload to the given file descriptor
    bool writePayload(types::i32 fd, Payload &&payload);

}  // namespace my_redis::payload
