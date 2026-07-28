# Tiny fixture libraries for the M4 (C/C++ interop) e2e tests - real,
# separately-compiled static libs to link against, not just calls into
# ebc's own runtime. Placed in a fixed subdirectory of the build tree so
# run_case.sh can point ebc at it via -L without guessing CMake's default
# archive output location.
set(EBASIC_FIXTURE_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/fixtures)

add_library(ebasic_fixture_c STATIC fixtures/c/fixture.c)
set_target_properties(ebasic_fixture_c PROPERTIES
    OUTPUT_NAME ebfixturec
    ARCHIVE_OUTPUT_DIRECTORY ${EBASIC_FIXTURE_LIB_DIR}
)

add_library(ebasic_fixture_cpp STATIC fixtures/cpp/fixture.cpp)
set_target_properties(ebasic_fixture_cpp PROPERTIES
    OUTPUT_NAME ebfixturecpp
    ARCHIVE_OUTPUT_DIRECTORY ${EBASIC_FIXTURE_LIB_DIR}
)

add_test(
    NAME e2e_hello_world
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/hello_world
)

add_test(
    NAME e2e_arithmetic
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/arithmetic
)

add_test(
    NAME e2e_types_and_operators
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/types_and_operators
)

add_test(
    NAME e2e_control_flow
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/control_flow
)

add_test(
    NAME e2e_const_enum_arrays_preprocessor
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/const_enum_arrays_preprocessor
)

add_test(
    NAME e2e_sub_function
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/sub_function
)

add_test(
    NAME e2e_redim
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/redim
)

add_test(
    NAME e2e_gosub
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/gosub
)

add_test(
    NAME e2e_include
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/include
)

add_test(
    NAME e2e_type_records
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/type_records
)

add_test(
    NAME e2e_namespace
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/namespace
)

add_test(
    NAME e2e_pointers
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/pointers
)

add_test(
    NAME e2e_unions
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/unions
)

add_test(
    NAME e2e_classes
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/classes
)

add_test(
    NAME e2e_inheritance
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/inheritance
)

add_test(
    NAME e2e_properties
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/properties
)

add_test(
    NAME e2e_operators
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/operators
)
