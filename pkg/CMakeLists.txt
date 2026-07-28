# toml++ (marzer/tomlplusplus) - vendored as a single header rather than
# fetched via FetchContent, so configuring this project never needs network
# access (matches every other target so far, including Haiku/CI machines
# that may not have reliable network at build time).
add_library(tomlplusplus INTERFACE)
target_include_directories(tomlplusplus INTERFACE ${CMAKE_SOURCE_DIR}/third_party/tomlplusplus)

add_executable(ebpm
    src/main.cpp
    src/manifest.cpp
    src/build.cpp
    src/resolve.cpp
    src/lockfile.cpp
    src/gitdep.cpp
)
target_link_libraries(ebpm PRIVATE tomlplusplus ebasic_process)
