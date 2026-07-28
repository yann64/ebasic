# A tiny shared static lib for the fork/exec subprocess helper
# (runProcess) - split out of ebc so pkg/ (ebpm, M5) can reuse it too rather
# than either duplicating this file or reaching across component
# boundaries for one source file.
add_library(ebasic_process STATIC
    src/driver/process.cpp
)
target_include_directories(ebasic_process PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(ebc
    src/driver/main.cpp
    src/diagnostics/diagnostics.cpp
    src/lexer/lexer.cpp
    src/preprocessor/preprocessor.cpp
    src/parser/parser.cpp
    src/sema/sema.cpp
    src/codegen/codegen.cpp
)

target_include_directories(ebc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ebc PRIVATE ebasic_process)

target_compile_definitions(ebc PRIVATE
    EBASIC_RUNTIME_INCLUDE_DIR="${CMAKE_SOURCE_DIR}/runtime/include"
)
