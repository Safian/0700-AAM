cmake_minimum_required(VERSION 3.10)
project(kick_mpc_gui C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(kick_mpc_gui main_gui.cpp)

find_package(Threads REQUIRED)
target_link_libraries(kick_mpc_gui PRIVATE Threads::Threads asound m)
