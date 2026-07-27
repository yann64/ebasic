add_library(ebasic-rt INTERFACE)
target_include_directories(ebasic-rt INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ebasic-rt INTERFACE cxx_std_17)
