#pragma once

#include "types.hpp"
#include <vector>

namespace my_redis
{
    struct Response
    {
        enum class Status : types::i32
        {
            RES_OK = 0,
            RES_ERR,
            RES_NX
        } status = Status::RES_OK;

        std::vector<types::u8> data;

        friend constexpr const char *statusStr(const Status &status) noexcept
        {
            switch (status)
            {
                case Status::RES_OK: return "OK";
                case Status::RES_ERR: return "ERR";
                case Status::RES_NX: return "NX";
                default: return "UNKNOWN";
            }
        }
    };
} // namespace my_redis