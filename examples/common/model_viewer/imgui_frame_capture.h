// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_IMGUI_FRAME_CAPTURE_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_IMGUI_FRAME_CAPTURE_H_

#include "model_viewer/frame_canvas_data.h"

#include <granit/integrations/imgui/renderer.hpp>

namespace granit::example::model_viewer {

/** 在 UI 线程把 ImGui 输出复制并解析为不再借用 ImGui 状态的 Canvas 数据。 */
[[nodiscard]] granit::result
capture_imgui_frame(const ImDrawData* draw_data,
                    granit::integration::imgui::texture_resolver resolver, void* user_data,
                    frame_canvas_data& output) noexcept;

} // namespace granit::example::model_viewer

#endif
