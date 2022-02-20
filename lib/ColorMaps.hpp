#pragma once

enum class ColorMap
{
    Viridis,
    Inferno,
    Magma,
    Plasma,
};

void upload_color_maps();

class Buffer;
Buffer& get_color_map(ColorMap color_map);
