# FLOꟼ

An MIT-licensed image viewer equipped with a GPU-accelerated perceptual image diffing algorithm based
on [ꟻLIP](https://research.nvidia.com/publication/2020-07_FLIP).

The tool is usable either as a standalone executable for comparing images, or as a library to programmatically
compare images and retrieve a comparison summary and write results to disk.

*Minor note regarding software build and runtime requirements*: This software was written and tested on one machine,
with a certain set of capabilities afforded to it. Additional testing and possible changes are needed
to verify correct operation on other drivers/platforms/hardware before relaxing requirements.

[Demo](demo.mp4)

## Build prerequisites

- Relatively modern C++ compiler (some C++20 features are used)
- CMake 3.21+
- VulkanSDK 1.2.198.1

## Build instructions

```bash
# Supply a custom generator with -G if you don't want the default one
# Pass -DFLOP_BUILD_EXE=OFF if you don't want to build the standalone executable
# Pass -DFLOP_BUILD_TESTS=ON if you want to compile and run the tests
cmake -B build -S .
cmake --build build
```

The first-time generation time will seem long because several dependencies are being fetched. The
dependencies used are:

- [GLFW](https://www.glfw.org/) (zlib/libpng)
- [Volk](https://github.com/zeux/volk) (MIT)
- [Dear ImGui](https://github.com/ocornut/imgui) (MIT)
- [DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler) (LLVM)
- [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) (MIT)
- [CLI11](https://github.com/CLIUtils/CLI11) (BSD 3-clause)
- [stb](https://github.com/nothings/stb) (MIT)

By default, both the library target and standalone executable are built. If you wish to link the
library against your own code, you may either link the `lflop` library target in cmake, or manually
link the compiled library through some other mechanism. The executable itself is simply `flop.exe`
(or just `flop` on Linux).

## Runtime requirements

- GPU with recent driver
- Windows 10+

Linux support should be relatively easy to provide, but is not yet tested. MacOS support will require integration
with the MoltenVK framework.

## Usage

The `flop.exe` can be run without any arguments and is a fully self-contained executable (no installation is required).
When launched in this way, an empty window with a few UI controls to select some images and begin the comparison as seen in the demo above.

Alternatively, command line options may be passed to optionally run FLOꟼ in headless mode, and supply image paths.
The full help text (obtainable by passing `-h` or `--help`) is reproduced below.

    An image comparison tool and visualizer
    Usage: FLOP [OPTIONS]
    
    Options:
      -h,--help                   Print this help message and exit
      -r,--reference TEXT         Path to reference image
      -t,--test TEXT              Path to test image
      -o TEXT                     Path to output file.
      --hl,--headless             Request that a gui not be presented

## Limitations

While FLOꟼ is implemented on the GPU, minimal profiling was actually done to fully optimize it.
Performance varies based on image size.
FLOꟼ currently doesn't properly account for differences in alpha, like the original paper.
The various kernel weights themselves are currently hardcoded in the shader bytecode. This may be relaxed in the future.
