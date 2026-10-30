cmake_minimum_required(VERSION 3.10)
project(kick_mpc_piano C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(kick_mpc_piano main.cpp)

find_package(Threads REQUIRED)
target_link_libraries(kick_mpc_piano PRIVATE Threads::Threads asound m)
