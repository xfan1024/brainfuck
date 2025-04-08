cmake_minimum_required(VERSION 3.10)
project(bf)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)

add_executable(bf bf.cpp bf_runner.cpp bf_runner_direct.cpp bf_runner_bf_insn.cpp bf_runner_jit.cpp)
target_include_directories(bf PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(bf PRIVATE ${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs support core irreader passes orcjit native)
target_link_libraries(bf ${llvm_libs})
