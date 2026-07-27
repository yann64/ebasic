add_executable(ebc
    src/driver/main.cpp
    src/driver/process.cpp
    src/diagnostics/diagnostics.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/sema/sema.cpp
    src/codegen/codegen.cpp
)

target_include_directories(ebc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(ebc PRIVATE
    EBASIC_RUNTIME_INCLUDE_DIR="${CMAKE_SOURCE_DIR}/runtime/include"
)
