add_executable(
    flop_tests
    Test.cpp
)

target_compile_features(
    flop_tests
    PUBLIC
    cxx_std_20
)

target_link_libraries(
    flop_tests
    PUBLIC
    lflop
)
