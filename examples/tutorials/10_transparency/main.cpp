// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "shader_archive.h"
#include "tutorial/tutorial_runtime.h"

#include <granit/renderer/pipeline.hpp>
#include <imgui.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>

namespace {
struct object_data { std::array<float, 4> position_size; std::array<float, 4> color_mode; };
constexpr std::uint32_t object_count = 4;
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
    if (result.ok()) result = shaders_.initialize(renderer(), tutorial_transparency::shader_archive());
    if (result.ok()) result = shaders_.create_shader("transparency.vertex", vertex_shader_);
    if (result.ok()) result = shaders_.create_shader("transparency.fragment", fragment_shader_);
    objects_data_ = {{{{-0.42F, 0.0F, 0.25F, 0.2F}, {0.95F, 0.2F, 0.1F, 1.0F}},
                       {{0.42F, 0.0F, 0.20F, 0.3F}, {0.1F, 0.7F, 1.0F, 0.45F}},
                       {{0.0F, 0.0F, 0.18F, 0.1F}, {1.0F, 0.85F, 0.1F, 0.25F}},
                       {{0.0F, -0.42F, 0.16F, 0.0F}, {0.2F, 1.0F, 0.3F, 0.65F}}}};
    if (result.ok()) result = objects_.initialize(renderer_owner(),
      {.size = sizeof(objects_data_), .usage = granit::buffer_usage::storage |
                                             granit::buffer_usage::transfer_destination});
    if (result.ok()) result = objects_.write(0, std::as_bytes(std::span{objects_data_}));
    if (result.ok()) result = initialize_layout();
    if (result.ok()) result = create_pipeline();
    return result;
  }
  granit::result on_swapchain_changed(const granit::swapchain_info& info) noexcept override {
    return info.format == pipeline_format_ ? granit::result::success : create_pipeline();
  }
  void on_shutdown(granit::result) noexcept override {
    runtime_.shutdown(); static_cast<void>(pipeline_.reset()); static_cast<void>(group_.reset());
    static_cast<void>(pipeline_layout_.reset()); static_cast<void>(layout_.reset());
    static_cast<void>(objects_.reset()); static_cast<void>(fragment_shader_.reset());
    static_cast<void>(vertex_shader_.reset()); static_cast<void>(shaders_.reset());
    static_cast<void>(frame_context_.reset()); static_cast<void>(depth_view_.reset());
    static_cast<void>(depth_.reset());
  }
  granit::result on_window_event(const granit::window_event& event) noexcept override {
    runtime_.process(event); return granit::result::success;
  }
  granit::result on_input_event(const granit::input_event& event) noexcept override {
    runtime_.process(event); return granit::result::success;
  }
  granit::result initialize_layout() noexcept {
    const std::array entries{granit::bind_group_layout_entry{.binding = 0,
      .type = granit::binding_type::storage_buffer, .visibility = granit::shader_stage_flags::vertex}};
    auto result = layout_.initialize(renderer_owner(), entries);
    const std::array layouts{layout_.ref()};
    if (result.ok()) result = pipeline_layout_.initialize(renderer_owner(), layouts);
    const std::array resources{granit::bind_group_entry{.binding = 0, .resource = objects_.ref(),
                                                        .size = sizeof(objects_data_)}};
    if (result.ok()) result = group_.initialize(renderer_owner(), layout_, resources);
    return result;
  }
  granit::result create_depth() noexcept {
    auto result = depth_view_.reset(); if (result.ok()) result = depth_.reset();
    if (result.ok()) result = depth_.initialize(renderer_owner(), {.format = granit::texture_format::d32_float,
      .usage = granit::texture_usage::depth_stencil_attachment, .width = presentation_info().width,
      .height = presentation_info().height});
    if (result.ok()) result = depth_view_.initialize(renderer_owner(), depth_);
    return result;
  }
  granit::result create_pipeline() noexcept {
    auto result = create_depth(); if (result.failed()) return result;
    result = pipeline_.reset(); if (result.failed()) return result;
    const auto format = presentation_info().format;
    const std::array blends{granit::color_blend_state{.source_color_factor = granit::blend_factor::source_alpha,
      .destination_color_factor = granit::blend_factor::one_minus_source_alpha,
      .source_alpha_factor = granit::blend_factor::one,
      .destination_alpha_factor = granit::blend_factor::one_minus_source_alpha}};
    result = pipeline_.initialize(renderer_owner(), {.layout = pipeline_layout_.ref(),
      .vertex_shader = vertex_shader_.ref(), .fragment_shader = fragment_shader_.ref(),
      .color_formats = std::span{&format, 1}, .depth_stencil_format = granit::texture_format::d32_float,
      .vertex_buffers = {}, .primitive = {}, .depth = granit::depth_state{.test_enabled = true,
      .write_enabled = false}, .color_blends = blends, .depth_bias = std::nullopt});
    if (result.ok()) pipeline_format_ = format; return result;
  }
  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    granit::window_state state; auto result = app_window().get_state(state);
    if (result.ok()) result = runtime_.begin_frame(state, frame.delta_seconds,
      {.name = "10 Transparency", .description = "Opaque, Mask, Blend and object sorting",
       .frame = rendered_frames()});
    if (result.ok()) { ImGui::Text("Order: opaque -> mask -> blend");
      ImGui::Text("OIT: unsupported; object sorting only"); result = runtime_.end_frame(); }
    if (result.failed()) return result;
    granit::frame_recording recording; result = frame_context_.begin(frame.acquired, recording);
    const granit::viewport viewport{0, 0, static_cast<float>(frame.swapchain.width),
                                    static_cast<float>(frame.swapchain.height), 0, 1};
    const granit::scissor scissor{0, 0, frame.swapchain.width, frame.swapchain.height};
    const granit::color_attachment_desc color{.view = frame.backbuffer.view, .resolve_view = {},
      .clear_value = {.red = 0.015F, .green = 0.02F, .blue = 0.04F, .alpha = 1.0F}};
    const granit::depth_stencil_attachment_desc depth{.view = depth_view_.ref(),
      .clear_value = {.depth = 1.0F}};
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
      .depth_stencil_attachment = &depth, .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
    auto& recorder = recording.recorder();
    if (result.ok()) result = recorder.set_viewports(0, std::span{&viewport, 1});
    if (result.ok()) result = recorder.set_scissors(0, std::span{&scissor, 1});
    if (result.ok()) result = recorder.bind_graphics_pipeline(pipeline_);
    if (result.ok()) result = recorder.bind_graphics_group(pipeline_layout_, 0, group_);
    if (result.ok()) result = recorder.begin_rendering(rendering);
    if (result.ok()) result = recorder.draw(object_count * 6U);
    if (result.ok()) result = recorder.end_rendering();
    if (result.ok()) result = runtime_.canvas().record(recorder, {.color = frame.backbuffer.view,
      .color_format = frame.swapchain.format, .width = frame.swapchain.width,
      .height = frame.swapchain.height, .load_operation = granit::attachment_load_operation::load,
      .encode_srgb = true, .frame_slot = recording.frame_slot()});
    if (result.ok()) result = recording.submit();
    if (result.failed() && recording.valid()) static_cast<void>(recording.abort()); return result;
  }
  granit::example::tutorial::tutorial_runtime runtime_; granit::frame_context frame_context_;
  granit::shader_library shaders_; granit::shader vertex_shader_, fragment_shader_; granit::buffer objects_;
  std::array<object_data, object_count> objects_data_{};
  granit::texture depth_; granit::texture_view depth_view_; granit::bind_group_layout layout_;
  granit::pipeline_layout pipeline_layout_; granit::bind_group group_; granit::graphics_pipeline pipeline_;
  granit::texture_format pipeline_format_{granit::texture_format::undefined}; bool smoke_test_{};
};
application app;
} // namespace

int main(int argc, char** argv) {
  const bool smoke = argc == 2 && std::string_view{argv[1]} == "--smoke-test"; app.set_smoke_test(smoke);
  const auto result = app.run({.executable_path = argc > 0 ? argv[0] : "", .title = "Granit Transparency",
    .renderer = {.application_name = "Granit Transparency", .presentation = granit::presentation_mode::enabled},
    .swapchain = {}, .smoke_test = smoke});
  if (smoke && result == granit::result::backend_unavailable) return 77;
  return result.failed() ? failure("application run", result) : 0;
}
