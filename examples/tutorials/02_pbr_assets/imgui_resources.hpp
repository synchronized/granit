// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TUTORIAL_02_IMGUI_RESOURCES_HPP_
#define GRANIT_TUTORIAL_02_IMGUI_RESOURCES_HPP_

#include <granit/granit.hpp>
#include <granit/integrations/imgui/renderer.hpp>
#include <imgui.h>

namespace tutorial_imgui {

inline constexpr ImTextureID font_texture_id = 1;
inline constexpr ImTextureID preview_texture_id = 2;

struct texture_binding {
  granit::texture_view_ref view;
  granit::sampler_ref sampler;
};

struct texture_bindings {
  texture_binding font;
  texture_binding preview;
};

[[nodiscard]] granit::result resolve_texture(ImTextureID id, granit::canvas_draw_state& state,
                                             void* user_data) noexcept;
[[nodiscard]] granit::result upload_font_atlas(granit::renderer& renderer, granit::texture& texture,
                                               granit::texture_view& view,
                                               granit::sampler& sampler);
[[nodiscard]] granit::result upload_checker(granit::renderer& renderer, granit::texture& texture,
                                            granit::texture_view& view);

} // namespace tutorial_imgui

#endif
