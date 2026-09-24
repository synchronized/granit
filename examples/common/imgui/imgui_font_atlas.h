// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_IMGUI_FONT_ATLAS_H_
#define GRANIT_EXAMPLES_COMMON_IMGUI_FONT_ATLAS_H_

#include "imgui/imgui_texture_registry.h"

#include <granit/granit.hpp>

namespace granit::example::imgui {

/** 上传当前 ImGui 字体图集，并将其注册到示例 Texture ID 表。 */
[[nodiscard]] granit::result initialize_font_atlas(granit::renderer& renderer,
                                                   texture_registry& registry,
                                                   granit::texture& texture,
                                                   granit::texture_view& view,
                                                   granit::sampler& sampler);

} // namespace granit::example::imgui

#endif
