# A tiny shared static lib for the fork/exec subprocess helper
# (runProcess) - split out of ebc so pkg/ (ebpm, M5) can reuse it too rather
# than either duplicating this file or reaching across component
# boundaries for one source file.
add_library(ebasic_process STATIC
    src/driver/process.cpp
)
target_include_directories(ebasic_process PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# M7: the compiler's front end (lexing/preprocessing/parsing/diagnostics,
# producing a real Module AST) - split out of ebc so docgen/ can reuse it
# too, mirroring the exact ebasic_process precedent above. Deliberately
# excludes sema/codegen: docgen only ever needs a parsed Module (every
# declared type is already resolved structurally by the parser itself), not
# a type-checked or lowered one.
add_library(ebasic_frontend STATIC
    src/diagnostics/diagnostics.cpp
    src/lexer/lexer.cpp
    src/preprocessor/preprocessor.cpp
    src/parser/parser.cpp
)
target_include_directories(ebasic_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(ebc
    src/driver/main.cpp
    src/sema/sema.cpp
    src/codegen/codegen.cpp
)

target_include_directories(ebc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ebc PRIVATE ebasic_process ebasic_frontend)

target_compile_definitions(ebc PRIVATE
    # M8e: two candidate locations for the runtime headers/PCH are baked
    # in - the build-tree path (used unconditionally until now) as a
    # fallback, and a path *relative to ebc's own executable* for the
    # installed case (see main.cpp's resolveRuntimeDirs) - a real package
    # (Haiku or otherwise) can't rely on the build tree still existing.
    EBASIC_RUNTIME_INCLUDE_DIR="${CMAKE_SOURCE_DIR}/runtime/include"
    EBASIC_RUNTIME_PCH_DIR="${EBASIC_RUNTIME_PCH_DIR}"
    # Relative path from the installed executable's own directory
    # (${CMAKE_INSTALL_BINDIR}) to the installed runtime headers'
    # directory - computed from GNUInstallDirs so it stays correct
    # regardless of a platform's own bin/data-directory-naming convention
    # (Haiku's own "data" vs the generic "share" - see the top-level
    # CMakeLists.txt's comment).
    EBASIC_RUNTIME_INSTALL_RELDIR="../${CMAKE_INSTALL_DATADIR}/ebasic/runtime"
)
if(TARGET ebasic_runtime_pch)
    add_dependencies(ebc ebasic_runtime_pch)
endif()

install(TARGETS ebc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
