cmake_minimum_required(VERSION 3.21)

project(flop)

option(FLOP_ENABLE_TESTS OFF "Build tests")
option(FLOP_BUILD_EXE ON "Build standalone executable")
option(FLOP_DISABLE_CONSOLE ON "Use the WinMain entry point on windows")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(shaders)
add_subdirectory(lib)

if(FLOP_BUILD_EXE)
    add_subdirectory(src)
endif()

if(FLOP_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
