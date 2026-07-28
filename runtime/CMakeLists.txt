add_library(ebasic-rt INTERFACE)
target_include_directories(ebasic-rt INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ebasic-rt INTERFACE cxx_std_17)

# M6: precompile the runtime header into a GCC .gch, built into the CMake
# *build* tree (never the source tree) at
# ${CMAKE_BINARY_DIR}/runtime_pch/ebasic/runtime/runtime.hpp.gch. ebc passes
# this directory as an extra -I *before* the real runtime include dir
# (compiler/CMakeLists.txt + main.cpp) - GCC automatically prefers a
# same-named .gch sitting in an earlier -I entry over reparsing the real
# header, with zero special flags needed at the ebc call site, and falls
# back silently (still correct, just slower) if the .gch doesn't match the
# invoking compiler's version/flags. This is GCC-only by design (M6, per
# plan): Clang uses a different file format and needs an explicit
# -include-pch flag, so it simply never looks at this .gch at all -
# confirmed harmless under `-cxx clang++`, just no speedup. Deliberately
# invokes literal "g++", not ${CMAKE_CXX_COMPILER} - ebc's own driver
# defaults to literal "g++" at runtime (main.cpp's `cxx = envCxx ? envCxx :
# "g++"`) regardless of which compiler built ebc itself, so the PCH must
# match *that* invocation, not this build's own C++ toolchain.
find_program(GXX_EXECUTABLE g++)
if(GXX_EXECUTABLE)
    set(EBASIC_RUNTIME_PCH_DIR ${CMAKE_BINARY_DIR}/runtime_pch CACHE INTERNAL "")
    set(EBASIC_RUNTIME_PCH_FILE ${EBASIC_RUNTIME_PCH_DIR}/ebasic/runtime/runtime.hpp.gch)
    add_custom_command(
        OUTPUT ${EBASIC_RUNTIME_PCH_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EBASIC_RUNTIME_PCH_DIR}/ebasic/runtime
        COMMAND ${GXX_EXECUTABLE} -std=c++17 -I ${CMAKE_CURRENT_SOURCE_DIR}/include
                -x c++-header ${CMAKE_CURRENT_SOURCE_DIR}/include/ebasic/runtime/runtime.hpp
                -o ${EBASIC_RUNTIME_PCH_FILE}
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/include/ebasic/runtime/runtime.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/ebasic/runtime/bstring.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/ebasic/runtime/print.hpp
        COMMENT "Precompiling ebasic-rt runtime header (M6 PCH)"
        VERBATIM
    )
    add_custom_target(ebasic_runtime_pch DEPENDS ${EBASIC_RUNTIME_PCH_FILE})
else()
    message(STATUS "g++ not found - skipping M6 runtime PCH (ebc will still work, just without the speedup)")
    # Always defined (even when empty) so compiler/CMakeLists.txt and
    # main.cpp never need to special-case "was a PCH configured at all" -
    # an empty/missing -I directory is already a harmless no-op.
    set(EBASIC_RUNTIME_PCH_DIR "" CACHE INTERNAL "")
endif()
