add_executable(ebasic_lsp
    src/main.cpp
    src/rpc.cpp
    src/documents.cpp
    src/server.cpp
    src/uri.cpp
    src/diagnostics.cpp
)
target_include_directories(ebasic_lsp PRIVATE
    ${CMAKE_SOURCE_DIR}/compiler/src
    ${CMAKE_SOURCE_DIR}/pkg/src
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(ebasic_lsp PRIVATE ebasic_frontend ebasic_sema ebasic_pkg nlohmann_json)

install(TARGETS ebasic_lsp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
