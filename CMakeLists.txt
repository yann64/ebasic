cmake_minimum_required(VERSION 3.20)
project(ebasic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(runtime)
add_subdirectory(compiler)

enable_testing()
add_subdirectory(tests)
