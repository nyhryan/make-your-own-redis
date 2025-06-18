cmake_minimum_required(VERSION 3.28)
project("my_redis"
    VERSION 1.0.0
    DESCRIPTION "A simple Redis client and server implementation in C++"
    LANGUAGES CXX
)

add_subdirectory("client")
add_subdirectory("server")