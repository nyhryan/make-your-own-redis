#pragma once

#include "types.hpp"
#include "util.hpp"

#include <stdexcept>
#include <string>

#include <cstring>

namespace my_redis::exception
{
    class errno_exception : public std::runtime_error
    {
    public:
        explicit errno_exception(std::string func, i32 errnum) noexcept
            : std::runtime_error{func + util::strerror(errnum)}
        {
        }

        errno_exception(const errno_exception &) = delete;
        errno_exception(errno_exception &&) = delete;
    };

}  // namespace my_redis::exception