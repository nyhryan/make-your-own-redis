#pragma once

#include <string>
#include <cstring>

namespace my_redis::util
{

    // Returns a string representation of errno errors
    inline std::string strerror(int errnum) noexcept
    {
        auto name = std::string{strerrorname_np(errnum)};
        auto desc = std::string{strerrordesc_np(errnum)};
        return name + ": " + desc;
    }

}  // namespace my_redis::util