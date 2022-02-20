#pragma once

#include <Image.hpp>

#include "ColorMaps.hpp"

struct GLFWwindow;

struct Rect2D
{
    int x1;
    int y1;
    int x2;
    int y2;
};

class Preview
{
public:
    enum class Quadrant
    {
        TopLeft     = 0,
        TopRight    = 1,
        BottomRight = 2,
        BottomLeft  = 3,
    };

    // Initialize pipelines, descriptor sets, etc.
    static void init(VkRenderPass render_pass);

    void set_quadrant(Quadrant quadrant);
    void set_image(Image& image);
    void set_viewport(Rect2D viewport);
    void set_color_map(ColorMap color_map);
    void render(GLFWwindow* window, VkCommandBuffer cb);
    Image const* image() const
    {
        return image_;
    }

    // Magnify or minify the image, attemping to keep the position under the
    // cursor fixed
    void zoom(GLFWwindow* window, bool magnify);
    void activate_pan(GLFWwindow* window);
    void deactivate_pan();

private:
    void viewport_extent(int& vw, int& vh) const;
    float viewport_aspect() const;
    float max_scale() const;
    float min_scale() const;

    Image* image_                = nullptr;
    Rect2D viewport_             = {};
    VkViewport preview_viewport_ = {};
    VkRect2D preview_scissor_    = {};
    float uv_offset_[2]          = {0.f, 0.f};
    float uv_scale_              = 1.f;
    double cursor_[2]            = {0, 0};
    float old_uv_offset_[2]      = {0.f, 0.f};
    uint32_t color_map_          = 0;
    bool use_color_map_          = false;
    bool pan_active_             = false;
    Quadrant quadrant_           = Quadrant::TopLeft;
};
