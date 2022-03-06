#include "UI.hpp"

#include <VkGlobals.hpp>
#include <algorithm>
#include <backends/imgui_impl_vulkan.h>
#include <cmath>
#include <imgui.h>

using namespace flop;

static nfdfilteritem_t s_filter_list[] = {{"Images", "png,jpg,jpeg,bmp,exr"}};
static nfdfilteritem_t s_output_list[]    = {{"PNG", "png"}};


extern void flop_init_reference(char const* reference_path);
extern void flop_init_test(char const* test_path);
extern int flop_analyze_impl(char const* reference_path,
                             char const* test_path,
                             char const* output_path,
                             float exposure,
                             int tonemapper,
                             FlopSummary* out_summary,
                             bool bypass_initialization);

void UI::set_reference(std::string const& reference)
{
    if (!reference.empty())
    {
        reference_path_ = const_cast<char*>(reference.data());
    }
}

void UI::set_test(std::string const& test)
{
    if (!test.empty())
    {
        test_path_ = const_cast<char*>(test.data());
    }
}

void UI::set_output(std::string const& output)
{
    if (!output.empty())
    {
        output_path_ = const_cast<char*>(output.data());
    }
}

void UI::on_scroll(GLFWwindow* window, bool magnify)
{
    error_preview_.zoom(window, magnify);
    left_preview_.zoom(window, magnify);
    right_preview_.zoom(window, magnify);
}

void UI::on_click(GLFWwindow* window, int button, int action)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            error_preview_.activate_pan(window);
            left_preview_.activate_pan(window);
            right_preview_.activate_pan(window);
        }
        else
        {
            error_preview_.deactivate_pan();
            left_preview_.deactivate_pan();
            right_preview_.deactivate_pan();
        }
    }
}

void UI::set_tonemap(Tonemap tonemap)
{
    tonemap_ = tonemap;
}

void UI::set_exposure(float exposure)
{
    exposure_ = exposure;
}

bool UI::analyze(bool issued_from_gui)
{
    bool disabled = !(reference_path_ && test_path_);
    if (disabled)
    {
        return false;
    }

    if (issued_from_gui)
    {
        if (flop_analyze_impl(reference_path_,
                              test_path_,
                              output_path_,
                              exposure_,
                              static_cast<int>(tonemap_),
                              &summary_,
                              true))
        {
            error_  = flop_get_error();
            active_ = false;
            return false;
        }
    }
    else
    {
        if (flop_analyze_hdr(reference_path_,
                             test_path_,
                             output_path_,
                             exposure_,
                             static_cast<int>(tonemap_),
                             &summary_))
        {
            error_  = flop_get_error();
            active_ = false;
            return false;
        }
    }

    error_ = nullptr;
    error_preview_.set_image(g_error);
    error_preview_.set_quadrant(Preview::Quadrant::TopLeft);
    error_preview_.set_color_map(color_map_);
    viewport_dirty_ = true;
    active_         = true;
    error_max_      = 0.f;
    for (size_t i = 0; i != 32; ++i)
    {
        error_histogram_[i] = static_cast<uint32_t*>(g_error_histogram.data_)[i];

        if (error_histogram_[i] > error_max_)
        {
            error_max_ = error_histogram_[i];
        }
    }

    if (issued_from_gui)
    {
        ImGui::CloseCurrentPopup();
    }
    return true;
}

void UI::update()
{
    ImGui::NewFrame();
    ImVec2 size = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos(ImVec2{size.x * 0.5f, 0.f});
    ImGui::SetNextWindowSize(ImVec2{size.x * 0.5f, size.y * 0.5f});

    if (ImGui::Begin("Flop",
                     nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar
                         | ImGuiWindowFlags_NoResize))
    {
        bool disabled = !(reference_path_ && test_path_);
        ImGui::BeginDisabled(disabled);

        if (ImGui::Button("Start analysis"))
        {
            analyze(true);
        }

        ImGui::EndDisabled();

        if (error_)
        {
            ImGui::Text("%s", error_);
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Columns(3);
        ImGui::Text("%s", reference_path_ ? reference_path_ : "(None selected)");
        if (ImGui::Button("Select reference image"))
        {
            NFD_OpenDialog(&reference_path_, s_filter_list, 1, nullptr);
            if (reference_path_)
            {
                flop_init_reference(reference_path_);
                toggled_ = false;
                left_preview_.set_image(g_reference.source_);
                left_preview_.set_quadrant(Preview::Quadrant::BottomLeft);
                viewport_dirty_ = true;
                active_         = false;

                if (g_reference.source_.width_ != g_test.source_.width_
                    || g_reference.source_.height_ != g_test.source_.height_)
                {
                    g_test.source_.reset();
                    g_error.reset();
                }
            }
        }
        ImGui::NextColumn();

        ImGui::Text("%s", test_path_ ? test_path_ : "(None selected)");
        if (ImGui::Button("Select test image"))
        {
            NFD_OpenDialog(&test_path_, s_filter_list, 1, nullptr);
            if (test_path_)
            {
                flop_init_test(test_path_);
                toggled_ = false;
                right_preview_.set_image(g_test.source_);
                right_preview_.set_quadrant(Preview::Quadrant::BottomRight);
                viewport_dirty_ = true;
                active_         = false;

                if (g_reference.source_.width_ != g_test.source_.width_
                    || g_reference.source_.height_ != g_test.source_.height_)
                {
                    g_reference.source_.reset();
                    g_error.reset();
                }
            }
        }

        ImGui::NextColumn();
        ImGui::Text(
            "%s", output_path_ ? output_path_ : "(No save file selected)");
        if (ImGui::Button("Select output location"))
        {
            NFD_SaveDialog(&output_path_, s_output_list, 1, nullptr, "flop.png");
        }

        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Separator();

        bool hdr = g_reference.source_.hdr_;
        ImGui::BeginDisabled(!hdr);

        if (ImGui::SliderFloat("Exposure", &exposure_, -15.f, 3.f))
        {
            float exposure = std::powf(2.f, exposure_);
            left_preview_.set_exposure(exposure);
            right_preview_.set_exposure(exposure);
        }

        Tonemap previous_tonemap = tonemap_;
        ImGui::Text("Tonemapping operator");
        if (ImGui::RadioButton("ACES", tonemap_ == Tonemap::ACES))
        {
            tonemap_ = Tonemap::ACES;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Reinhard", tonemap_ == Tonemap::Reinhard))
        {
            tonemap_ = Tonemap::Reinhard;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Hable", tonemap_ == Tonemap::Hable))
        {
            tonemap_ = Tonemap::Hable;
        }

        if (tonemap_ != previous_tonemap)
        {
            left_preview_.set_tonemap(tonemap_);
            right_preview_.set_tonemap(tonemap_);
        }

        ImGui::EndDisabled();

        ImGui::Checkbox("Toggle positions", &toggled_);
        ImGui::SameLine();
        if (ImGui::Checkbox("Animate toggle", &animated_))
        {
            if (animated_)
            {
                last_toggled_ = glfwGetTime();
            }
            else
            {
                left_preview_.set_image(g_reference.source_);
            }
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Toggle interval (s)", &toggle_interval_s_, 0.1f, 3.f);

        ImGui::Spacing();

        if (active_)
        {
            if (animated_)
            {
                if (left_preview_.image() == &g_reference.source_)
                {
                    ImGui::Text("Viewing reference image.");
                }
                else
                {
                    ImGui::Text("Viewing test image.");
                }
            }
            else if (toggled_)
            {
                ImGui::Text(
                    "Viewing test image on the left, reference image on "
                    "the right.");
            }
            else
            {
                ImGui::Text(
                    "Viewing reference image on the left, test image on "
                    "the right.");
            }

            ImGui::Spacing();

            if (ImGui::RadioButton("Magma", color_map_ == ColorMap::Magma))
            {
                error_preview_.set_color_map(ColorMap::Magma);
                color_map_ = ColorMap::Magma;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Viridis", color_map_ == ColorMap::Viridis))
            {
                error_preview_.set_color_map(ColorMap::Viridis);
                color_map_ = ColorMap::Viridis;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Plasma", color_map_ == ColorMap::Plasma))
            {
                error_preview_.set_color_map(ColorMap::Plasma);
                color_map_ = ColorMap::Plasma;
            }

            ImGui::Text("Reference/Test Image View Mode");
            if (ImGui::RadioButton("Source", view_mode_ == ViewMode::Source))
            {
                view_mode_ = ViewMode::Source;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(
                    "Filtered Source", view_mode_ == ViewMode::FilteredSource))
            {
                view_mode_ = ViewMode::FilteredSource;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("YyCxCz", view_mode_ == ViewMode::YyCxCz))
            {
                view_mode_ = ViewMode::YyCxCz;
            }

            ImGui::Spacing();

            ImGui::PlotHistogram("Error histogram",
                                 error_histogram_,
                                 32,
                                 0,
                                 nullptr,
                                 0.f,
                                 error_max_,
                                 ImVec2(std::min(size.x / 3.4f, 300.f),
                                        std::min(size.y / 4, 140.f)),
                                 4);
        }
    }
    ImGui::End();
}

void UI::render(GLFWwindow* window, VkCommandBuffer cb)
{
    ImGuiViewport& viewport = *ImGui::GetMainViewport();
    if (reference_path_ && !error_)
    {
        if (viewport_dirty_)
        {
            Rect2D preview_viewport;
            preview_viewport.x1 = 0;
            preview_viewport.y1 = viewport.WorkPos.y;
            preview_viewport.x2 = viewport.WorkSize.x;
            preview_viewport.y2 = viewport.WorkSize.y + preview_viewport.y1;
            left_preview_.set_viewport(preview_viewport);
            right_preview_.set_viewport(preview_viewport);
            error_preview_.set_viewport(preview_viewport);
            viewport_dirty_ = false;
        }

        if (animated_)
        {
            double now = glfwGetTime();
            if (now - last_toggled_ > toggle_interval_s_)
            {
                last_toggled_ = now;
                toggled_      = !toggled_;
            }
        }
        else
        {
            right_preview_.render(window, cb);
        }
        ImagePacket& left  = toggled_ ? g_test : g_reference;
        ImagePacket& right = toggled_ ? g_reference : g_test;

        switch (view_mode_)
        {
            using enum ViewMode;
        case Source:
            left_preview_.set_image(left.source_);
            right_preview_.set_image(right.source_);
            break;
        case YyCxCz:
            left_preview_.set_image(left.yycxcz_);
            right_preview_.set_image(right.yycxcz_);
            break;
        case FilteredSource:
            left_preview_.set_image(left.yycxcz_blurred_);
            right_preview_.set_image(right.yycxcz_blurred_);
            break;
        default:
            left_preview_.set_image(left.source_);
            right_preview_.set_image(right.source_);
            break;
        }
        left_preview_.render(window, cb);
        error_preview_.render(window, cb);
    }

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);
}

void UI::mark_viewport_dirty()
{
    viewport_dirty_ = true;
}
