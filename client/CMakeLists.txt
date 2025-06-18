set(EXE "client")
add_executable(${EXE} "src/main.cpp")

target_include_directories(${EXE} PRIVATE 
    "include"
    "../common/include"
)

target_sources(${EXE} PRIVATE
    "../common/src/socket.cpp"
    "../common/src/payload.cpp"
)

set_target_properties(${EXE} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    EXPORT_COMPILE_COMMANDS ON
)