add_library(ebasic-rt INTERFACE)
target_include_directories(ebasic-rt INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ebasic-rt INTERFACE cxx_std_17)

# M6: the runtime header's precompiled form(s), built into the CMake
# *build* tree (never the source tree) under
# ${CMAKE_BINARY_DIR}/runtime_pch/ebasic/runtime/ - one toolchain-specific
# file per backend compiler ebc might be told to use (-cxx/CXX), since
# only one toolchain's file is ever relevant to any single ebc invocation
# (no collision risk keeping both under the same directory). Always set
# (even when neither block below actually runs) so compiler/CMakeLists.txt
# and main.cpp never need to special-case "was a PCH dir configured at
# all" - an empty/missing directory entry is already a harmless no-op.
set(EBASIC_RUNTIME_PCH_DIR ${CMAKE_BINARY_DIR}/runtime_pch CACHE INTERNAL "")

# GCC: precompile into a real .gch. ebc passes this directory as an extra
# -I *before* the real runtime include dir (compiler/CMakeLists.txt +
# main.cpp) - GCC automatically prefers a same-named .gch sitting in an
# earlier -I entry over reparsing the real header, with zero special
# flags needed at the ebc call site, and falls back silently (still
# correct, just slower) if the .gch doesn't match the invoking compiler's
# version/flags. Clang uses a different file format and needs an explicit
# -include-pch flag, so it simply never looks at this .gch at all -
# confirmed harmless under `-cxx clang++`, just no speedup. Deliberately
# invokes literal "g++", not ${CMAKE_CXX_COMPILER} - ebc's own driver
# defaults to literal "g++" at runtime (main.cpp's `cxx = envCxx ? envCxx :
# "g++"`) regardless of which compiler built ebc itself, so the PCH must
# match *that* invocation, not this build's own C++ toolchain.
find_program(GXX_EXECUTABLE g++)
if(GXX_EXECUTABLE)
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
        COMMENT "Precompiling ebasic-rt runtime header (M6 PCH, GCC)"
        VERBATIM
    )
    add_custom_target(ebasic_runtime_pch DEPENDS ${EBASIC_RUNTIME_PCH_FILE})
else()
    message(STATUS "g++ not found - skipping M6 runtime PCH (GCC) - ebc will still work, just without the speedup")
endif()

# MSVC: precompile into a real .pch via /Yc. Unlike GCC's automatic
# same-directory lookup above, cl.exe needs explicit /Yu+/Fp flags on
# every consuming compile (see main.cpp's runtimeIncludeArgs, which
# supplies them whenever this .pch exists) and can't precompile a bare
# header file the way `g++ -x c++-header` can - it needs a real "PCH
# source" .cpp whose first #include is the header being precompiled
# (runtime/pch/runtime_pch.cpp). The PCH-creation compile also always
# produces a companion .obj; it's never linked into anything (the runtime
# headers are declarations/inline only - the same reason the GCC .gch
# above needs no .o counterpart at all) and is excluded from install()
# below. Deliberately invokes literal "cl", not ${CMAKE_CXX_COMPILER},
# mirroring the GCC block's own reasoning above.
find_program(CL_EXECUTABLE cl)
if(CL_EXECUTABLE)
    set(EBASIC_RUNTIME_PCH_MSVC_FILE ${EBASIC_RUNTIME_PCH_DIR}/ebasic/runtime/runtime.pch)
    set(EBASIC_RUNTIME_PCH_MSVC_OBJ ${EBASIC_RUNTIME_PCH_DIR}/ebasic/runtime/runtime_pch.obj)
    add_custom_command(
        OUTPUT ${EBASIC_RUNTIME_PCH_MSVC_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EBASIC_RUNTIME_PCH_DIR}/ebasic/runtime
        COMMAND ${CL_EXECUTABLE} /nologo /std:c++17 /EHsc
                /I ${CMAKE_CURRENT_SOURCE_DIR}/include
                /c ${CMAKE_CURRENT_SOURCE_DIR}/pch/runtime_pch.cpp
                /Ycebasic/runtime/runtime.hpp
                "/Fp${EBASIC_RUNTIME_PCH_MSVC_FILE}"
                "/Fo${EBASIC_RUNTIME_PCH_MSVC_OBJ}"
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/pch/runtime_pch.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/ebasic/runtime/runtime.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/ebasic/runtime/bstring.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/ebasic/runtime/print.hpp
        COMMENT "Precompiling ebasic-rt runtime header (M6 PCH, MSVC)"
        VERBATIM
    )
    add_custom_target(ebasic_runtime_pch_msvc DEPENDS ${EBASIC_RUNTIME_PCH_MSVC_FILE})
else()
    message(STATUS "cl.exe not found - skipping M6 runtime PCH (MSVC) - ebc will still work, just without the speedup")
endif()

# M8e: installs the headers (and any PCH(s) that were built above) under
# the same "ebasic/runtime/{include,pch}" substructure ebc's own
# EBASIC_RUNTIME_INSTALL_RELDIR (compiler/CMakeLists.txt) expects to find
# relative to wherever it's actually installed - see main.cpp's
# resolveRuntimeDirs for the runtime-side lookup this pairs with. The
# MSVC PCH-creation object (see above) is never a real runtime artifact -
# excluded so it's never installed.
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ebasic/runtime/include)
if(GXX_EXECUTABLE OR CL_EXECUTABLE)
    install(DIRECTORY ${EBASIC_RUNTIME_PCH_DIR}/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ebasic/runtime/pch
            PATTERN "*.obj" EXCLUDE)
endif()
