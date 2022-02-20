# FileEmbed.cmake
# LICENSE MIT
# Reproduced from https://gitlab.com/jhamberg/cmake-examples/-/blob/master/cmake/FileEmbed.cmake
# with modifications to support C++

get_filename_component(base_filename ${INPUT_PATH} NAME)
string(MAKE_C_IDENTIFIER ${base_filename} c_name)
file(READ ${INPUT_PATH} content HEX)

# Separate into individual bytes.
string(REGEX MATCHALL "([A-Fa-f0-9][A-Fa-f0-9])" SEPARATED_HEX ${content})

set(output_c "")

set(counter 0)
foreach (hex IN LISTS SEPARATED_HEX)
    string(APPEND output_c "0x${hex},")
    MATH(EXPR counter "${counter}+1")
    if (counter GREATER 16)
        string(APPEND output_c "\n    ")
        set(counter 0)
    endif ()
endforeach ()

set(output_c "
#include \"${c_name}.h\"
uint8_t ${c_name}_data[] = {
    ${output_c}
}\;
unsigned ${c_name}_size = sizeof(${c_name}_data)\;
")

set(output_h "
#ifndef ${c_name}_H
#define ${c_name}_H
#include \"stdint.h\"
#ifdef __cplusplus
extern \"C\" {
#endif
    extern uint8_t ${c_name}_data[]\;
    extern unsigned ${c_name}_size\;
#ifdef __cplusplus
}
#endif
#endif // ${c_name}_H
")

file(WRITE ${OUTPUT_PATH}/${c_name}.c ${output_c})
file(WRITE ${OUTPUT_PATH}/${c_name}.h ${output_h})
