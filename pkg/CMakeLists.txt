# toml++ (marzer/tomlplusplus) - vendored as a single header rather than
# fetched via FetchContent, so configuring this project never needs network
# access (matches every other target so far, including Haiku/CI machines
# that may not have reliable network at build time).
add_library(tomlplusplus INTERFACE)
target_include_directories(tomlplusplus INTERFACE ${CMAKE_SOURCE_DIR}/third_party/tomlplusplus)

# LSP: manifest parsing + dependency-graph resolution + build-flag
# computation, split out of ebpm the same way ebasic_process/ebasic_frontend
# were split out of ebc - so the new ebasic_lsp (lsp/) can call the real
# resolveDependencyGraph/computeConsumerDirs in-process to resolve
# #include "dep.iface.bas" exactly like a real `ebpm build` would, rather
# than re-implementing (and risking drifting from) dependency resolution.
add_library(ebasic_pkg STATIC
    src/manifest.cpp
    src/build.cpp
    src/resolve.cpp
    src/lockfile.cpp
    src/gitdep.cpp
    src/semver.cpp
)
target_link_libraries(ebasic_pkg PUBLIC tomlplusplus ebasic_process)
target_include_directories(ebasic_pkg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(ebpm
    src/main.cpp
)
target_link_libraries(ebpm PRIVATE ebasic_pkg)
target_include_directories(ebpm PRIVATE ${CMAKE_BINARY_DIR}/generated)

install(TARGETS ebpm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# REG-1: a small, plain assertion-based unit test for semver.{hpp,cpp} -
# no existing unit-test-binary precedent exists in this codebase (every
# other test drives a real compiled tool end-to-end), but SemVer has no
# natural CLI surface of its own until REG-4/REG-6 land - see
# semver_test.cpp's own doc comment for why a standalone binary is the
# simplest honest way to verify it before anything else depends on it.
add_executable(semver_test src/semver_test.cpp)
target_link_libraries(semver_test PRIVATE ebasic_pkg)
