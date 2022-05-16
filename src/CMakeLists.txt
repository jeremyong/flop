# External dependencies

set(CLI11_BUILD_DOCS OFF CACHE BOOL "")
set(CLI11_BUILD_TESTS OFF CACHE BOOL "")
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "")
set(CLI11_BUILD_EXAMPLES_JSON OFF CACHE BOOL "")
set(CLI11_INSTALL OFF CACHE BOOL "")
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.1.2
    GIT_SHALLOW ON
)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")
FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/releases/download/3.3.6/glfw-3.3.6.zip
)

FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG 28ade5a5cc5d17cea8fe4034572cac8fd54eb53f
    GIT_SHALLOW ON
)

FetchContent_MakeAvailable(cli11 glfw nfd)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.86
    GIT_SHALLOW ON
)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(
    limgui
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)
target_compile_features(
    limgui
    PRIVATE
    cxx_std_17
)
target_include_directories(limgui
    PUBLIC ${imgui_SOURCE_DIR}
    PRIVATE ${imgui_SOURCE_DIR}/backends)
target_link_libraries(limgui PUBLIC glfw volk)
target_compile_definitions(limgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES VK_NO_PROTOTYPES)

add_executable(flop)

target_sources(flop
    PUBLIC
    Main.cpp
    ImGuiVulkan.cpp
    Preview.cpp
    Preview.hpp
    UI.cpp
    UI.hpp
)

target_include_directories(
    flop
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_compile_features(
    flop
    PUBLIC
    cxx_std_20
)

target_compile_definitions(
    flop
    PUBLIC
    NOMINMAX=1
)

target_link_libraries(
    flop
    PUBLIC
    CLI11::CLI11
    glfw
    lflop
    limgui
    nfd
)
