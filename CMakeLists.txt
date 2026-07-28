cmake_minimum_required(VERSION 3.20)
# C is enabled for tests/fixtures/c's tiny fixture library, used by the M4
# C/C++ interop e2e tests to link against something real and reproducible.
project(ebasic LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(runtime)
add_subdirectory(compiler)
add_subdirectory(pkg)
add_subdirectory(docgen)

enable_testing()
add_subdirectory(tests)

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/docs/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen into build/docs/html"
        VERBATIM
    )
endif()
