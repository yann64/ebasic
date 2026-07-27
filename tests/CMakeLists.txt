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
