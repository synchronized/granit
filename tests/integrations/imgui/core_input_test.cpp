// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/integrations/imgui/input.hpp>

#include <imgui.h>

#include <catch2/catch_all.hpp>

TEST_CASE("core ImGui frame input has no Window dependency", "[imgui][input]") {
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  unsigned char* pixels{};
  int width{};
  int height{};
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  granit::integration::imgui::begin_frame({
      .display_width = 640.0F,
      .display_height = 480.0F,
      .delta_seconds = 1.0F / 60.0F,
  });
  CHECK(io.DisplaySize.x == 640.0F);
  CHECK(io.DisplaySize.y == 480.0F);
  ImGui::EndFrame();
  ImGui::DestroyContext();
}
