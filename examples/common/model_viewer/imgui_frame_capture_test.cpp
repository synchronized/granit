// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/imgui_frame_capture.h"

#include <catch2/catch_all.hpp>

namespace {

granit::result resolve_texture(ImTextureID, granit_canvas_draw_state&, void*) noexcept {
  return granit::result::success;
}

} // namespace

TEST_CASE("ImGui 帧捕获拒绝空输入并清理旧数据") {
  granit::example::model_viewer::frame_canvas_data output;
  output.indices.push_back(7);

  CHECK(granit::example::model_viewer::capture_imgui_frame(
            nullptr, resolve_texture, nullptr, output) == granit::result::invalid_argument);
  CHECK(output.indices.empty());
}

TEST_CASE("ImGui 帧捕获接受没有绘制命令的有效帧") {
  ImDrawData draw_data;
  draw_data.Valid = true;
  granit::example::model_viewer::frame_canvas_data output;

  CHECK(granit::example::model_viewer::capture_imgui_frame(&draw_data, resolve_texture, nullptr,
                                                           output)
            .ok());
  CHECK(output.empty());
}
