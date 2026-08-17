// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INTEGRATIONS_IMGUI_RENDERER_HPP_
#define GRANIT_INTEGRATIONS_IMGUI_RENDERER_HPP_

#include <imgui.h>

#include <granit/integrations/imgui/export.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>

namespace granit::integration::imgui {

using texture_resolver = result (*)(ImTextureID texture, granit_canvas_draw_state& state,
                                    void* user_data) noexcept;

/** 把 ImGui Draw Data 追加到 Canvas；纹理与采样器由调用方的 resolver 映射。 */
[[nodiscard]] GRANIT_INTEGRATION_IMGUI_API result
append_draw_data(const ImDrawData* draw_data, canvas_draw_list& canvas, texture_resolver resolver,
                 void* user_data = nullptr) noexcept;

} // namespace granit::integration::imgui

#endif
