// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/integrations/imgui/input.hpp>
#include <granit/integrations/imgui/window_input.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_all.hpp>

#include <cstring>

namespace {

class imgui_context {
public:
  imgui_context() {
    ImGui::CreateContext();
    unsigned char* pixels{};
    int width{};
    int height{};
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~imgui_context() { ImGui::DestroyContext(); }
  imgui_context(const imgui_context&) = delete;
  imgui_context& operator=(const imgui_context&) = delete;
};

} // namespace

TEST_CASE("imgui integration begins a frame without Window", "[imgui][input]") {
  imgui_context context;

  granit::integration::imgui::begin_frame({
      .display_width = 800.0F,
      .display_height = 600.0F,
      .framebuffer_scale_x = 2.0F,
      .framebuffer_scale_y = 1.5F,
      .delta_seconds = 0.02F,
  });

  const auto& io = ImGui::GetIO();
  REQUIRE(io.DisplaySize.x == 800.0F);
  REQUIRE(io.DisplaySize.y == 600.0F);
  REQUIRE(io.DisplayFramebufferScale.x == 2.0F);
  REQUIRE(io.DisplayFramebufferScale.y == 1.5F);
  REQUIRE(io.DeltaTime == 0.02F);
  ImGui::EndFrame();
}

TEST_CASE("window input adapter forwards keyboard text and pointer events", "[imgui][input]") {
  imgui_context context;

  granit::input_event key{};
  key.type = granit::input_event_type::key;
  key.data.key.physical = granit::physical_key::a;
  key.data.key.modifiers = GRANIT_MODIFIER_LEFT_SHIFT_BIT;
  key.data.key.action = granit::key_action::pressed;
  granit::integration::imgui::window::process_input_event(key);

  granit::input_event text{};
  text.type = granit::input_event_type::text;
  text.data.text.length = 3;
  std::memcpy(text.data.text.utf8, "\xE4\xB8\xAD", 3);
  granit::integration::imgui::window::process_input_event(text);

  granit::input_event pointer{};
  pointer.type = granit::input_event_type::pointer_moved;
  pointer.data.pointer_moved = {
      .x = 120.0F,
      .y = 80.0F,
  };
  granit::integration::imgui::window::process_input_event(pointer);

  pointer.type = granit::input_event_type::pointer_button;
  pointer.data.pointer_button = {
      .x = 120.0F,
      .y = 80.0F,
      .button = GRANIT_POINTER_PRIMARY_BIT,
      .pressed = 1,
  };
  granit::integration::imgui::window::process_input_event(pointer);

  granit::input_event wheel{};
  wheel.type = granit::input_event_type::pointer_wheel;
  wheel.data.pointer_wheel.delta_y = 2.0F;
  granit::integration::imgui::window::process_input_event(wheel);
  REQUIRE(GImGui->InputEventsQueue.Size >= 5);

  granit::integration::imgui::begin_frame(
      {.display_width = 800.0F, .display_height = 600.0F, .delta_seconds = 1.0F / 60.0F});

  const auto& io = ImGui::GetIO();
  REQUIRE(ImGui::IsKeyDown(ImGuiKey_A));
  REQUIRE(ImGui::IsKeyDown(ImGuiMod_Shift));
  REQUIRE(io.InputQueueCharacters.Size == 1);
  ImGui::EndFrame();
}

TEST_CASE("window input adapter forwards focus and pointer leave", "[imgui][input]") {
  imgui_context context;

  granit::window_event focus{};
  focus.type = granit::window_event_type::focus_changed;
  focus.data.focus.focused = false;
  granit::integration::imgui::window::process_window_event(focus);

  granit::input_event leave{};
  leave.type = granit::input_event_type::pointer_left;
  granit::integration::imgui::window::process_input_event(leave);
  granit::integration::imgui::begin_frame(
      {.display_width = 800.0F, .display_height = 600.0F, .delta_seconds = 1.0F / 60.0F});

  const auto& io = ImGui::GetIO();
  REQUIRE(io.AppFocusLost);
  REQUIRE(io.MousePos.x < -100000.0F);
  ImGui::EndFrame();
}
