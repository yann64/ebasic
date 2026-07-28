cmake_minimum_required(VERSION 3.20)
# C is enabled for tests/fixtures/c's tiny fixture library, used by the M4
# C/C++ interop e2e tests to link against something real and reproducible.
project(ebasic VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Version reporting: PROJECT_VERSION (above) is the single source of truth;
# the short git commit hash is best-effort, degrading gracefully to empty
# when there's no .git (e.g. a HaikuPorts build from a source tarball, or
# scripts/haiku_verify.sh's own `git archive` deploy, which never includes
# .git) or no git binary at all.
set(EBASIC_GIT_HASH "")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE EBASIC_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _ebasic_git_rc
    )
    if(NOT _ebasic_git_rc EQUAL 0)
        set(EBASIC_GIT_HASH "")
    endif()
endif()
configure_file(
    ${CMAKE_SOURCE_DIR}/compiler/src/driver/version.hpp.in
    ${CMAKE_BINARY_DIR}/generated/ebasic/version.hpp
    @ONLY
)

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

# nlohmann/json - vendored as a single header (third_party/nlohmann_json),
# same rationale as pkg/CMakeLists.txt's own tomlplusplus vendoring: no
# network access needed at configure time. Used by lsp/'s own JSON-RPC
# transport.
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json)
add_subdirectory(lsp)

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
