cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)
add_compile_definitions(UNICODE)
add_compile_definitions(_UNICODE)

project ("equals")

add_executable (equals WIN32 "main.cpp" "crc32.cpp" "crc32.h"  "tcp.h")
