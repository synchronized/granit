// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "tutorial/tutorial_runtime.h"

#include <granit/renderer/renderer.hpp>
#include <imgui.h>

#include <iostream>
#include <span>
#include <string_view>

namespace {

int failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

class application final : public granit::example::application {
public:
  void set_smoke_test(bool value) noexcept { smoke_test_ = value; }

private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.ok()) result = frame_context_.initialize(renderer());
    if (result.ok()) result = renderer_owner().get_limits(limits_);
    return result;
  }

  granit::result on_swapchain_changed(const granit::swapchain_info&) noexcept override {
    return granit::result::success;
  }

  void on_shutdown(granit::result) noexcept override { runtime_.shutdown(); }

  granit::result on_window_event(const granit::window_event& event) noexcept override {
    runtime_.process(event);
    return granit::result::success;
  }

  granit::result on_input_event(const granit::input_event& event) noexcept override {
    runtime_.process(event);
    return granit::result::success;
  }

  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    granit::window_state state;
    auto result = app_window().get_state(state);
    if (result.ok()) {
      result = runtime_.begin_frame(state, frame.delta_seconds,
                                    {.name = "12 Bindless Probe",
                                     .description = "Descriptor Indexing capability and fallback",
                                     .frame = rendered_frames()});
    }
    if (result.ok()) {
      ImGui::Text("Descriptor Indexing device support: %s",
                  limits_.supports_bindless_descriptor_indexing() ? "yes" : "no");
      ImGui::Text("Granit Bindless backend: experimental / not active");
      ImGui::Text("Default path: traditional Bind Group");
      ImGui::Text("WebGPU and unsupported Vulkan devices: explicit fallback");
      result = runtime_.end_frame();
    }
    if (result.failed()) return result;

    granit::frame_recording recording;
    result = frame_context_.begin(frame.acquired, recording);
    const granit::color_attachment_desc color{.view = frame.backbuffer.view,
                                              .resolve_view = {},
                                              .clear_value = {.red = 0.015F,
                                                              .green = 0.02F,
                                                              .blue = 0.04F,
                                                              .alpha = 1.0F}};
    const granit::rendering_desc rendering{
        .color_attachments = std::span{&color, 1},
        .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
    if (result.ok()) result = recording.recorder().begin_rendering(rendering);
    if (result.ok()) result = recording.recorder().end_rendering();
    if (result.ok()) {
      result = runtime_.canvas().record(
          recording.recorder(), {.color = frame.backbuffer.view,
                                 .color_format = frame.swapchain.format,
                                 .width = frame.swapchain.width,
                                 .height = frame.swapchain.height,
                                 .load_operation = granit::attachment_load_operation::load,
                                 .encode_srgb = true,
                                 .frame_slot = recording.frame_slot()});
    }
    if (result.ok()) result = recording.submit();
    if (result.failed() && recording.valid()) static_cast<void>(recording.abort());
    return result;
  }

  granit::example::tutorial::tutorial_runtime runtime_;
  granit::frame_context frame_context_;
  granit::renderer_limits limits_{};
  bool smoke_test_{};
};

application app;
} // namespace

int main(int argc, char** argv) {
  const bool smoke = argc == 2 && std::string_view{argv[1]} == "--smoke-test";
  app.set_smoke_test(smoke);
  const auto result = app.run({.executable_path = argc > 0 ? argv[0] : "",
                               .title = "Granit Bindless Probe",
                               .renderer = {.application_name = "Granit Bindless Probe",
                                            .presentation = granit::presentation_mode::enabled},
                               .swapchain = {},
                               .smoke_test = smoke});
  if (smoke && result == granit::result::backend_unavailable) return 77;
  return result.failed() ? failure("application run", result) : 0;
}
