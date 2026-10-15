// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "tutorial/tutorial_runtime.h"
#include <array>
#include <cstdint>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/texture.hpp>
#include <imgui.h>
#include <iostream>
#include <span>
#include <string_view>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {
struct format_row {
  const char* name;
  granit::texture_format format;
};
constexpr std::array formats{
    format_row{"RGBA8", granit::texture_format::rgba8_unorm},
    format_row{"RGBA16F", granit::texture_format::rgba16_float},
    format_row{"D32", granit::texture_format::d32_float},
    format_row{"BC7", granit::texture_format::bc7_rgba_unorm},
};
int failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}
class application final : public granit::example::application {
public:
  void set_smoke_test(bool value) noexcept { smoke_test_ = value; }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return runtime_.canvas_items(); }
  [[nodiscard]] std::uint32_t format_count() const noexcept { return format_count_; }

private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.ok())
      result = frame_context_.initialize(renderer());
    if (result.ok())
      result = renderer_owner().get_info(renderer_info_);
    if (result.ok())
      result = renderer_owner().get_limits(limits_);
    for (const auto& row : formats) {
      granit::texture_format_capabilities capabilities;
      if (result.ok() &&
          granit::get_texture_format_capabilities(renderer_owner(), row.format, capabilities).ok())
        capabilities_[format_count_++] = capabilities;
    }
    if (result.ok()) {
      const granit::texture_desc desc{.format = granit::texture_format::rgba8_unorm,
                                      .usage = granit::texture_usage::sampled |
                                               granit::texture_usage::transfer_destination,
                                      .width = 4,
                                      .height = 4,
                                      .mip_levels = 2};
      mip_texture_result_ = mip_texture_.initialize(renderer_owner(), desc);
      mip_texture_created_ = mip_texture_result_.ok();
      sampler_result_ = sampler_.initialize(renderer_owner(), {.min_lod = 0.0F, .max_lod = 1.0F});
      sampler_created_ = sampler_result_.ok();
      const std::array<std::byte, sizeof(granit::draw_indirect_args)> indirect_data{};
      indirect_buffer_result_ = indirect_buffer_.initialize(
          renderer_owner(), {.size = indirect_data.size(), .usage = granit::buffer_usage::indirect},
          indirect_data);
      indirect_buffer_created_ = indirect_buffer_result_.ok();
    }
    return result;
  }
  granit::result on_swapchain_changed(const granit::swapchain_info&) noexcept override {
    return granit::result::success;
  }
  void on_shutdown(granit::result) noexcept override {
    runtime_.shutdown();
    static_cast<void>(indirect_buffer_.reset());
    static_cast<void>(sampler_.reset());
    static_cast<void>(mip_texture_.reset());
  }
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
    if (result.ok())
      result = runtime_.begin_frame(state, frame.delta_seconds,
                                    {.name = "11 Capabilities",
                                     .description = "Format, usage, sampler and renderer limits",
                                     .frame = rendered_frames()});
    if (result.ok()) {
      ImGui::SeparatorText("Renderer");
      ImGui::Text("Adapter: %s", renderer_info_.adapter_name.c_str());
      ImGui::Text("Uniform alignment: %llu", limits_.uniform_buffer_offset_alignment);
      ImGui::Text("Max uniform binding: %llu", limits_.max_uniform_buffer_binding_size);
      ImGui::Text("Max color attachments: %u", limits_.max_color_attachments);
      ImGui::Text("Max anisotropy: %.1f", limits_.max_sampler_anisotropy);
      ImGui::Text("Framebuffer sample mask: 0x%X", limits_.framebuffer_sample_counts);

      ImGui::SeparatorText("Feature Support");
      ImGui::Text("Timestamp queries: %s", limits_.supports_timestamp_queries() ? "yes" : "no");
      ImGui::Text("Async readback: %s", limits_.supports_async_readback() ? "yes" : "no");
      ImGui::Text("Pipeline warmup: %s", limits_.supports_pipeline_warmup() ? "yes" : "no");
      ImGui::Text("Non-blocking warmup: %s",
                  limits_.supports_non_blocking_pipeline_warmup() ? "yes" : "no");
      ImGui::Text("Bindless descriptors: %s",
                  limits_.supports_bindless_descriptor_indexing() ? "yes" : "no");

      ImGui::SeparatorText("Texture Format Capabilities");
      if (ImGui::BeginTable("format_capabilities", 4, ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("Format");
        ImGui::TableSetupColumn("Sampled");
        ImGui::TableSetupColumn("Color");
        ImGui::TableSetupColumn("Filterable");
        ImGui::TableHeadersRow();
        for (std::uint32_t index = 0; index < format_count_; ++index) {
          const auto& row = formats[index];
          const auto& value = capabilities_[index];
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextUnformatted(row.name);
          ImGui::TableSetColumnIndex(1);
          ImGui::TextUnformatted(value.supports(granit::texture_usage::sampled) ? "yes" : "no");
          ImGui::TableSetColumnIndex(2);
          ImGui::TextUnformatted(value.supports(granit::texture_usage::color_attachment) ? "yes"
                                                                                         : "no");
          ImGui::TableSetColumnIndex(3);
          ImGui::TextUnformatted(value.filterable() ? "yes" : "no");
        }
        ImGui::EndTable();
      }

      ImGui::SeparatorText("Resource Probes");
      ImGui::Text("2-level sampled texture: %s", mip_texture_created_ ? "created" : "unsupported");
      ImGui::Text("Sampler: %s", sampler_created_ ? "created" : "unsupported");
      ImGui::Text("Indirect buffer: %s", indirect_buffer_created_ ? "created" : "unsupported");
      result = runtime_.end_frame();
    }
    if (result.failed())
      return result;
    granit::frame_recording recording;
    result = frame_context_.begin(frame.acquired, recording);
    const granit::color_attachment_desc color{
        .view = frame.backbuffer.view,
        .resolve_view = {},
        .clear_value = {.red = 0.015F, .green = 0.02F, .blue = 0.04F, .alpha = 1.0F}};
    const granit::rendering_desc rendering{
        .color_attachments = std::span{&color, 1},
        .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
    if (result.ok())
      result = recording.recorder().begin_rendering(rendering);
    if (result.ok())
      result = recording.recorder().end_rendering();
    if (result.ok())
      result = runtime_.canvas().record(recording.recorder(),
                                        {.color = frame.backbuffer.view,
                                         .color_format = frame.swapchain.format,
                                         .width = frame.swapchain.width,
                                         .height = frame.swapchain.height,
                                         .load_operation = granit::attachment_load_operation::load,
                                         .encode_srgb = true,
                                         .frame_slot = recording.frame_slot()});
    if (result.ok())
      result = recording.submit();
    if (result.failed() && recording.valid()) {
      static_cast<void>(recording.abort());
    }
    return result;
  }
  granit::example::tutorial::tutorial_runtime runtime_;
  granit::frame_context frame_context_;
  granit::renderer_info renderer_info_{};
  granit::renderer_limits limits_{};
  std::array<granit::texture_format_capabilities, formats.size()> capabilities_{};
  granit::texture mip_texture_;
  granit::sampler sampler_;
  granit::buffer indirect_buffer_;
  granit::result mip_texture_result_{granit::result::invalid_argument};
  granit::result sampler_result_{granit::result::invalid_argument};
  granit::result indirect_buffer_result_{granit::result::invalid_argument};
  std::uint32_t format_count_{};
  bool smoke_test_{};
  bool mip_texture_created_{};
  bool sampler_created_{};
  bool indirect_buffer_created_{};
};
application app;
} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_11_rendered_frames() noexcept {
  return app.rendered_frames();
}
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_11_recreate_count() noexcept {
  return app.completed_recreates();
}
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_11_feature_value() noexcept {
  return app.format_count();
}
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_11_canvas_items() noexcept {
  return app.canvas_items();
}
extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_11_ready() noexcept {
  return app.ready() ? 1 : 0;
}
#endif

int main(int argc, char** argv) {
  const bool smoke = argc == 2 && std::string_view{argv[1]} == "--smoke-test";
  app.set_smoke_test(smoke);
  const auto result = app.run({.executable_path = argc > 0 ? argv[0] : "",
                               .title = "Granit Capabilities",
                               .renderer = {.application_name = "Granit Capabilities",
                                            .presentation = granit::presentation_mode::enabled},
                               .swapchain = {},
                               .smoke_test = smoke});
  if (smoke && result == granit::result::backend_unavailable)
    return 77;
  return result.failed() ? failure("application run", result) : 0;
}
