#pragma once

// This interface is designed to be usable by C and C++ runtimes (or any
// language with a C-ABI compatible FFI)
#include <cstdint>

#ifdef __cplusplus
extern "C"
{
#endif

    struct FlopSummary
    {
        int width;
        int height;
        int milliseconds_elapsed;
    };

    // Call to retrieve a C-string describing the last encountered error
    char const* flop_get_error();

    void flop_config_enable_validation();

    // Prepare the flop runtime for image analysis.
    // Returns 0 on success, 1 on failure.
    int flop_init(uint32_t instanceExtensionCount,
                  char const** requiredInstanceExtensions);

    // Compare the left and right images, and write out a summary of the
    // analysis
    int flop_analyze(char const* image_left_path,
                     char const* image_right_path,
                     // The output path is optional, and if not supplied, no
                     // readback is performed
                     char const* output_path,
                     FlopSummary* out_summary);

#ifdef __cplusplus
} // extern "C"
#endif
