// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_IMGUI_SAMPLE_RESOURCES_H_
#define GRANIT_EXAMPLES_COMMON_IMGUI_SAMPLE_RESOURCES_H_

#include <imgui.h>

#include <granit/granit.hpp>
#include <granit/integrations/imgui/renderer.hpp>

namespace granit::example {

inline constexpr ImTextureID imgui_font_texture_id = 1;
inline constexpr ImTextureID imgui_checker_texture_id = 2;

struct imgui_texture_binding {
  granit_texture_view view{GRANIT_NULL_HANDLE};
  granit_sampler sampler{GRANIT_NULL_HANDLE};
};

struct imgui_sample_texture_bindings {
  imgui_texture_binding font;
  imgui_texture_binding checker;
};

[[nodiscard]] result resolve_imgui_sample_texture(ImTextureID texture,
                                                  granit_canvas_draw_state& state,
                                                  void* user_data) noexcept;

[[nodiscard]] result upload_imgui_checker_texture(granit_renderer renderer, texture& output,
                                                  texture_view& view);

[[nodiscard]] result upload_imgui_font_atlas(granit_renderer renderer, texture& output,
                                             texture_view& view, sampler& output_sampler);

[[nodiscard]] bool imgui_target_needs_srgb_encoding(texture_format format) noexcept;

/** 将已转换的 ImGui Canvas 录制到当前颜色附件。 */
[[nodiscard]] result record_imgui_sample_canvas(command_recorder& recorder,
                                                canvas_draw_list& canvas,
                                                granit_texture_view target,
                                                const swapchain_info& info,
                                                std::uint32_t frame_slot) noexcept;

} // namespace granit::example

#endif
