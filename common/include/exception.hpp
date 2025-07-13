#pragma once

#include "types.hpp"
#include "util.hpp"

#include <string_view>
#include <format>
#include <stdexcept>
#include <cerrno>

namespace my_redis::exception
{
    class errno_exception : public std::runtime_error
    {
    public:
        errno_exception(std::string_view msg, types::i32 errnum) noexcept
            : std::runtime_error{std::format("{} : {}", msg, util::strerror(errnum))}
        {
        }

        explicit errno_exception(std::string_view msg) noexcept
            : errno_exception(msg, errno)
        {
        }

        errno_exception(const errno_exception &)    = delete;
        errno_exception(errno_exception &&)         = delete;
    };

}  // namespace my_redis::exception