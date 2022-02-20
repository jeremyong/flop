cmake_minimum_required(VERSION 3.21)
project(FlopShaders)

message(STATUS "Compiling the following spirv hex files: @FLOP_SPIRV_HEX@")

add_library(
    flop_shaders_hex
    STATIC
    ${FLOP_SPIRV_HEX})
