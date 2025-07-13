#pragma once

#include "types.hpp"
#include <vector>

namespace my_redis::buffer
{
    using buffer_t = std::vector<types::u8>;

    template <typename T>
    constexpr void append(buffer_t &buffer, T *data, types::size n)
    {
        buffer.insert(
            buffer.end(), 
            reinterpret_cast<const types::u8*>(data),
            reinterpret_cast<const types::u8*>(data) + n
        );
    }

    constexpr void consume(buffer_t &buffer, types::size n)
    {
        buffer.erase(buffer.begin(), buffer.begin() + n);
    }

} //  namespace my_redis::buffer