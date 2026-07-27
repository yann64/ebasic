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
