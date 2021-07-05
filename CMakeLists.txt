cmake_minimum_required(VERSION 3.19)
project(nvm CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
SET (CMAKE_CXX_COMPILER "/usr/bin/clang++")
add_compile_options(-Werror)
include_directories(~/neo/)

add_executable(nvm main.cpp Assembler.cpp NVMVirtualMachine.cpp)
