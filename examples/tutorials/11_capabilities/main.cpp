// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "tutorial/tutorial_runtime.h"
#include <granit/renderer/texture.hpp>
#include <imgui.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

namespace {
struct format_row { const char* name; granit::texture_format format; };
constexpr std::array formats{
  format_row{"RGBA8", granit::texture_format::rgba8_unorm},
  format_row{"RGBA16F", granit::texture_format::rgba16_float},
  format_row{"D32", granit::texture_format::d32_float},
  format_row{"BC7", granit::texture_format::bc7_rgba_unorm},
};
int failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n'; return 1;
}
class application final : public granit::example::application {
public:
  void set_smoke_test(bool value) noexcept { smoke_test_ = value; }
private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.ok()) result = frame_context_.initialize(renderer());
    if (result.ok()) result = renderer_owner().get_limits(limits_);
    for (const auto& row : formats) {
      granit::texture_format_capabilities capabilities;
      if (result.ok() && granit::get_texture_format_capabilities(renderer_owner(), row.format, capabilities).ok())
        capabilities_[format_count_++] = capabilities;
    }
    return result;
  }
  granit::result on_swapchain_changed(const granit::swapchain_info&) noexcept override { return granit::result::success; }
  void on_shutdown(granit::result) noexcept override { runtime_.shutdown(); }
  granit::result on_window_event(const granit::window_event& event) noexcept override { runtime_.process(event); return granit::result::success; }
  granit::result on_input_event(const granit::input_event& event) noexcept override { runtime_.process(event); return granit::result::success; }
  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    granit::window_state state; auto result = app_window().get_state(state);
    if (result.ok()) result = runtime_.begin_frame(state, frame.delta_seconds,
      {.name = "11 Capabilities", .description = "Format, usage, sampler and renderer limits", .frame = rendered_frames()});
    if (result.ok()) {
      ImGui::Text("Max color attachments: %u", limits_.max_color_attachments);
      ImGui::Text("Max anisotropy: %.1f", limits_.max_sampler_anisotropy);
      ImGui::Text("Timestamp queries: %s", limits_.supports_timestamp_queries() ? "yes" : "no");
      ImGui::Separator();
      for (std::uint32_t index = 0; index < format_count_; ++index) {
        const auto& row = formats[index]; const auto& value = capabilities_[index];
        ImGui::Text("%s: sampled=%s color=%s filterable=%s", row.name,
                    value.supports(granit::texture_usage::sampled) ? "yes" : "no",
                    value.supports(granit::texture_usage::color_attachment) ? "yes" : "no",
                    value.filterable() ? "yes" : "no");
      }
      result = runtime_.end_frame();
    }
    if (result.failed()) return result;
    granit::frame_recording recording; result = frame_context_.begin(frame.acquired, recording);
    const granit::color_attachment_desc color{.view = frame.backbuffer.view, .resolve_view = {},
      .clear_value = {.red = 0.015F, .green = 0.02F, .blue = 0.04F, .alpha = 1.0F}};
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
      .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
    if (result.ok()) result = recording.recorder().begin_rendering(rendering);
    if (result.ok()) result = recording.recorder().end_rendering();
    if (result.ok()) result = runtime_.canvas().record(recording.recorder(), {.color = frame.backbuffer.view,
      .color_format = frame.swapchain.format, .width = frame.swapchain.width, .height = frame.swapchain.height,
      .load_operation = granit::attachment_load_operation::load, .encode_srgb = true,
      .frame_slot = recording.frame_slot()});
    if (result.ok()) result = recording.submit();
    if (result.failed() && recording.valid()) static_cast<void>(recording.abort()); return result;
  }
  granit::example::tutorial::tutorial_runtime runtime_; granit::frame_context frame_context_;
  granit::renderer_limits limits_{}; std::array<granit::texture_format_capabilities, formats.size()> capabilities_{};
  std::uint32_t format_count_{}; bool smoke_test_{};
};
application app;
} // namespace
int main(int argc, char** argv) {
  const bool smoke = argc == 2 && std::string_view{argv[1]} == "--smoke-test"; app.set_smoke_test(smoke);
  const auto result = app.run({.executable_path = argc > 0 ? argv[0] : "", .title = "Granit Capabilities",
    .renderer = {.application_name = "Granit Capabilities", .presentation = granit::presentation_mode::enabled},
    .swapchain = {}, .smoke_test = smoke});
  if (smoke && result == granit::result::backend_unavailable) return 77;
  return result.failed() ? failure("application run", result) : 0;
}
