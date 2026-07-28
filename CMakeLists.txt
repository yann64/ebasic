cmake_minimum_required(VERSION 3.20)
# C is enabled for tests/fixtures/c's tiny fixture library, used by the M4
# C/C++ interop e2e tests to link against something real and reproducible.
project(ebasic LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# M8e: standard install-directory conventions (bin/, share/, ...) - used by
# every install() rule below. Haiku's own convention deviates from the
# generic GNU default (its real data directory is "data", not "share" -
# confirmed on real Haiku hardware: /boot/system/share exists but is empty,
# while /boot/system/data holds every other installed package's own data,
# e.g. CMake's own CMAKE_ROOT lands there - CMakePresets.json's "haiku"
# preset overrides CMAKE_INSTALL_DATADIR accordingly, since GNUInstallDirs
# itself has no Haiku-specific handling at all).
include(GNUInstallDirs)

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
