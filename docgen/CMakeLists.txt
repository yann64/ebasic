add_executable(docgen
    src/main.cpp
    src/render.cpp
)
target_include_directories(docgen PRIVATE ${CMAKE_SOURCE_DIR}/compiler/src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(docgen PRIVATE ebasic_frontend)

install(TARGETS docgen RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
