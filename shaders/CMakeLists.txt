include(FetchContent)

FetchContent_Declare(
    dxcompiler
    # Release info: https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.6.2112
    URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.6.2112/dxc_2021_12_08.zip
)
if(NOT dxcompiler_POPULATED)
    FetchContent_Populate(dxcompiler)
endif()

set(SHADER_BIN ${CMAKE_BINARY_DIR}/shader_lib)
if(NOT EXISTS ${SHADER_BIN})
    file(MAKE_DIRECTORY ${SHADER_BIN})
endif()

set(FLOP_SPIRV)
set(FLOP_SPIRV_HEX "")
set(CMAKE_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/BinToHex.cmake)
function(add_spv SOURCE TARGET PROFILE ENTRY)
    set(OUTFILE ${CMAKE_CURRENT_BINARY_DIR}/${TARGET})
    set(SOURCEFILE ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE})
    set(CMD_ARGS "-spirv -T ${PROFILE} -E ${ENTRY} -Fo \"${OUTFILE}\" ${ARGV4} \"${SOURCEFILE}\"")
    get_filename_component(BASE ${OUTFILE} NAME)
    string(MAKE_C_IDENTIFIER ${BASE} HEX_SOURCE)
    string(APPEND FLOP_SPIRV_HEX " ${HEX_SOURCE}.c")
    set(FLOP_SPIRV_HEX ${FLOP_SPIRV_HEX} PARENT_SCOPE)

    add_custom_command(
        OUTPUT ${OUTFILE}
        COMMAND ${dxcompiler_SOURCE_DIR}/bin/x64/dxc.exe -spirv -T ${PROFILE} -E ${ENTRY} -Fo ${OUTFILE} ${ARGV4} ${SOURCEFILE}
        COMMAND ${CMAKE_COMMAND} -DINPUT_PATH=${OUTFILE} -DOUTPUT_PATH=${SHADER_BIN} -P ${CMAKE_SCRIPT}
        MAIN_DEPENDENCY ${SOURCE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Compiling shader ${SOURCE}"
        VERBATIM
        USES_TERMINAL ON
    )
    list(APPEND FLOP_SPIRV ${OUTFILE})
    set(FLOP_SPIRV ${FLOP_SPIRV} PARENT_SCOPE)
endfunction()

add_spv(CSFFilter.hlsl CSFFilterX.spv cs_6_6 CSMain "-DDIRECTION_X")
add_spv(CSFFilter.hlsl CSFFilterY.spv cs_6_6 CSMain "-DDIRECTION_Y")
add_spv(ColorCompare.hlsl ColorCompare.spv cs_6_6 CSMain)
add_spv(ErrorColorMap.hlsl ErrorColorMap.spv ps_6_6 PSMain)
add_spv(FeatureFilter.hlsl FeatureFilterX.spv cs_6_6 CSMain "-DDIRECTION_X")
add_spv(FeatureFilter.hlsl FeatureFilterY.spv cs_6_6 CSMain "-DDIRECTION_Y")
add_spv(FullscreenVS.hlsl FullscreenVS.spv vs_6_6 VSMain)
add_spv(Preview.hlsl PreviewVS.spv vs_6_6 VSMain)
add_spv(Preview.hlsl PreviewPS.spv ps_6_6 PSMain)
add_spv(Preview.hlsl PreviewPSColorMap.spv ps_6_6 PSMain "-DCOLORMAP")
add_spv(Summarize.hlsl Summarize.spv cs_6_6 CSMain)
add_spv(Tonemap.hlsl Tonemap.spv ps_6_6 PSMain)
add_spv(YyCxCz.hlsl YyCxCz.spv ps_6_6 PSMain)

configure_file(HexToLib.cmake ${SHADER_BIN}/CMakeLists.txt)

add_custom_command(
    OUTPUT ${SHADER_BIN}/$<CONFIG>/flop_shaders_hex${CMAKE_STATIC_LIBRARY_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${SHADER_BIN}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${SHADER_BIN}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${SHADER_BIN}/Debug
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${SHADER_BIN}/Release
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${SHADER_BIN}/flop_shaders_hex.dir
    COMMAND ${CMAKE_COMMAND} -S ${SHADER_BIN} -B ${SHADER_BIN} -G ${CMAKE_GENERATOR}
    COMMAND ${CMAKE_COMMAND} --build ${SHADER_BIN} --target flop_shaders_hex --config $<CONFIG>
    DEPENDS ${FLOP_SPIRV}
)
add_custom_target(
    flop_shaders_hex_lib
    DEPENDS ${SHADER_BIN}/$<CONFIG>/flop_shaders_hex${CMAKE_STATIC_LIBRARY_SUFFIX})
add_library(flop_shaders INTERFACE)
target_link_libraries(
    flop_shaders
    INTERFACE ${SHADER_BIN}/$<CONFIG>/flop_shaders_hex${CMAKE_STATIC_LIBRARY_SUFFIX})
target_include_directories(
    flop_shaders
    INTERFACE ${SHADER_BIN}
)
add_dependencies(flop_shaders flop_shaders_hex_lib)
