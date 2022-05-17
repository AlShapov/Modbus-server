cmake_minimum_required(VERSION 3.16)
project(servtest)

set(CMAKE_CXX_STANDARD 14)

set(INC_DIR /usr/include/modbus)
set(LINK_DIR /usr/lib)

include_directories(${INC_DIR})
link_directories(${LINK_DIR})
link_libraries(modbus)

add_executable(servtest main.cpp)
target_link_libraries(servtest modbus)
