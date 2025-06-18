#pragma once

#include "types.hpp"
#include <cstring>
#include <string_view>

namespace my_redis::payload
{

    /*
        * Payload structure:
        * | Header (4 bytes)| Message (4096 bytes) | Null terminator (1 byte) |
        * The header contains the length of the message in bytes.
        * The message is a null-terminated string.
    */
    constexpr size HEADER_LEN = 4;
    constexpr size MSG_LEN = 4096;
    constexpr size PAYLOAD_LEN = HEADER_LEN + MSG_LEN + 1;

    class Payload
    {
        friend bool readPayload(i32 fd, Payload &payload);
        friend bool writePayload(i32 fd, Payload &payload);

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
        u32 messageLength() const
        {
            u32 length = 0;
            std::memcpy(&length, m_Buffer, HEADER_LEN);
            return length;
        }

        // Get the start address of the message
        const u8 *message() const { return m_Buffer + HEADER_LEN; }

    private:
        // Raw buffer to hold the payload
        u8 m_Buffer[PAYLOAD_LEN];
    };

    // Read a payload from the given file descriptor
    bool readPayload(i32 fd, Payload &payload);

    // Write a payload to the given file descriptor
    bool writePayload(i32 fd, Payload &payload);

}  // namespace my_redis::payload
