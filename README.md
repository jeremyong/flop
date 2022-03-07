# FLOꟼ

![image](https://user-images.githubusercontent.com/250149/156910917-7f61ba21-d8fb-44f0-8ec5-ebef4a64bbc0.png)

[Prebuilt Single-file Binaries](https://github.com/jeremyong/flop/releases) - currently Windows only

FLOꟼ (FLOP with a backwards P if rendered correctly) is an MIT-licensed image viewer equipped with a GPU-accelerated perceptual image diffing algorithm based
on [ꟻLIP](https://research.nvidia.com/publication/2020-07_FLIP). Read the accompanying blog post [here](https://www.jeremyong.com/color%20theory/2022/02/19/implementing-the-flip-algorithm/).

The tool is usable either as a standalone executable for comparing images, or as a library to programmatically
compare images and retrieve a comparison summary and write results to disk.

*Minor note regarding software build and runtime requirements*: This software was written and tested on one machine,
with a certain set of capabilities afforded to it. Additional testing and possible changes are needed
to verify correct operation on other drivers/platforms/hardware before relaxing requirements.

https://user-images.githubusercontent.com/250149/154824329-fd790c14-b228-4ca7-b384-3c372e72b09e.mp4

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
- [tinyexr](https://github.com/syoyo/tinyexr) (BSD 3-clause)

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

    An image comparison tool and visualizer. All options are optional unless headless mode is requested.
    Usage: FLOP [OPTIONS]
    
    Options:
      -h,--help                   Print this help message and exit
      -r,--reference TEXT         Path to reference image
      -t,--test TEXT              Path to test image
      -o,--output TEXT            Path to output file.
      -e,--exposure FLOAT         Exposure to apply to an HDR image (log 2 stops)
      --tonemapper ENUM:value in {ACES->1,Reinhard->2,Hable->3} OR {1,2,3}
                                  HDR to LDR tonemapping operator
      --hl,--headless             Request that a gui not be presented
      -f,--force                  Overwrite image if file exists at specified output path

FLOꟼ may also be used as a library. The `test/` folder demonstrates how to link and programmatically analyze LDR or HDR images.

## Differences from the original algorithm

The original paper assumes fully opaque color values, but it is sometimes useful to compare differences in images that possess an alpha channel.
FLOꟼ accommodates this by detecting the presence of the alpha channel and scaling the Yy component in linearized _Lab_ space prior to the constrast sensitivity filters.

The HDR FLOꟼ algorithm simply tonemaps both reference and test HDR inputs to LDR ranges with a specified global exposure.
This is done currently for speed (less than 100 ms on my machine), but an local tonemapping operator or exposure bracketing
may be an option in the future.

## Limitations

While FLOꟼ is implemented on the GPU, minimal profiling was actually done to fully optimize it.
Performance varies based on image size.
The various kernel weights themselves are currently hardcoded in the shader bytecode. This may be relaxed in the future.

The HDR-ꟻLIP algorithm doesn't require specifying exposure when HDR images are supplied. It operates by
automatically determining the exposure range, compute the ꟻLIP error for each exposure, then taking the maximum error per-pixel.
FLOꟼ on the other hand, is meant to be used interactively, so it's recommended that the GUI be used to determine the
appropriate exposure when analyzing HDR images.

## References

- [LDR ꟻLIP](https://research.nvidia.com/publication/2020-07_FLIP)
- [HDR ꟻLIP](https://research.nvidia.com/publication/2021-05_HDR-FLIP)
