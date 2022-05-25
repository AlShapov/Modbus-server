cmake_minimum_required(VERSION 3.16)
project(servtest)

set(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_FLAGS -pthread)
set(INC_DIR /usr/include/modbus)
set(LINK_DIR /usr/lib)

include_directories(${INC_DIR})
link_directories(${LINK_DIR})

set(INC_DIR /usr/include/yaml-cpp)
include_directories(${INC_DIR})
link_libraries(modbus yaml-cpp)

add_executable(servtest main.cpp)
target_link_libraries(servtest modbus yaml-cpp)

target_compile_options(servtest PRIVATE -fsanitize=address)
target_link_options(servtest PRIVATE -fsanitize=address)
