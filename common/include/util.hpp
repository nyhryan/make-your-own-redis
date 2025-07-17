#pragma once

#include <cstring>

#include <format>
#include <string>

namespace my_redis::util
{

    // Returns a string representation of errno errors
    constexpr std::string strerror(int errnum) noexcept
    {
        auto name = std::string{strerrorname_np(errnum)};
        auto desc = std::string{strerrordesc_np(errnum)};
        return std::format("{1}({0})", name, desc);
    }

}  // namespace my_redis::util