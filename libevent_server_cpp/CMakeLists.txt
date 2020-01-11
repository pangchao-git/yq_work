cmake_minimum_required(VERSION 3.5.1)
project(libevent_server_cpp)

set(CMAKE_CXX_STANDARD 14)

add_executable(libevent_server_cpp main.cpp cJSON.cpp cJSON.h)
target_link_libraries(libevent_server_cpp m event)