// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "tutorial/tutorial_runtime.h"

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/resource_table.hpp>
#include <imgui.h>

#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

int failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

class application final : public granit::example::application {
public:
  void set_smoke_test(bool value) noexcept { smoke_test_ = value; }
  void set_stress_test(bool value) noexcept { stress_test_ = value; }

private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.ok()) result = frame_context_.initialize(renderer());
    if (result.ok()) result = renderer_owner().get_limits(limits_);
    if (result.ok()) {
      result = granit::get_resource_table_capabilities(renderer_owner().ref(), table_capabilities_);
    }
    if (result.ok()) {
      result = table_.initialize(renderer_owner(),
                                 {.resource_types = GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT,
                                  .sampler_capacity = 1});
    }
    if (result.ok()) {
      result = sampler_.initialize(renderer_owner());
    }
    if (result.ok()) {
      result = table_.update({.type = granit::resource_table_resource_type::sampler,
                              .slot = 0,
                              .resource = sampler_.native_handle()});
    }
    if (result.ok() && stress_test_) {
      stress_tables_.resize(64);
      for (auto& table : stress_tables_) {
        result = table.initialize(renderer_owner(),
                                  {.resource_types = GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT,
                                   .sampler_capacity = 1});
        if (result.failed()) break;
        result = table.update({.type = granit::resource_table_resource_type::sampler,
                               .slot = 0,
                               .resource = sampler_.native_handle()});
        if (result.failed()) break;
      }
    }
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
                                    {.name = "12 Resource Table Scene",
                                     .description = "Resource Table scene and pressure validation",
                                     .frame = rendered_frames()});
    }
    if (result.ok()) {
      ImGui::Text("Descriptor Indexing device support: %s",
                  limits_.supports_bindless_descriptor_indexing() ? "yes" : "no");
      ImGui::Text("Resource Table support: %s", resource_table_support_name(table_capabilities_.support));
      ImGui::Text("Resource Table sampler slot 0: populated");
      ImGui::Text("Pressure tables: %zu", stress_tables_.size());
      if (stress_test_)
        ImGui::TextUnformatted("Stress mode: 64 tables with one sampler slot");
      ImGui::Text("Default path: traditional Bind Group");
      ImGui::Text("Explicit Resource Table binding remains experimental");
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
  granit::resource_table_capabilities table_capabilities_{};
  granit::resource_table table_;
  std::vector<granit::resource_table> stress_tables_;
  granit::sampler sampler_;
  bool smoke_test_{};
  bool stress_test_{};

  static const char* resource_table_support_name(granit::resource_table_support support) noexcept {
    switch (support) {
    case granit::resource_table_support::native:
      return "native";
    case granit::resource_table_support::logical:
      return "logical";
    case granit::resource_table_support::fallback:
      return "fallback";
    default:
      return "unsupported";
    }
  }
};

application app;
} // namespace

int main(int argc, char** argv) {
  const bool smoke = argc >= 2 && std::string_view{argv[1]} == "--smoke-test";
  const bool stress = argc >= 2 && std::string_view{argv[1]} == "--stress-test";
  app.set_smoke_test(smoke);
  app.set_stress_test(stress);
  const auto result = app.run({.executable_path = argc > 0 ? argv[0] : "",
                               .title = "Granit Resource Table Scene",
                               .renderer = {.application_name = "Granit Resource Table",
                                            .presentation = granit::presentation_mode::enabled},
                               .swapchain = {},
                               .smoke_test = smoke});
  if (smoke && result == granit::result::backend_unavailable) return 77;
  return result.failed() ? failure("application run", result) : 0;
}
