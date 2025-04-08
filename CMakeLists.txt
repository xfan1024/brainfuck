cmake_minimum_required(VERSION 3.10)
project(bf)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bf bf.cpp bf_runner.cpp bf_runner_direct.cpp bf_runner_bf_insn.cpp)
