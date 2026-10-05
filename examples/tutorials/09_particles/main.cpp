// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "shader_archive.h"
#include "tutorial/tutorial_runtime.h"

#include <granit/renderer/pipeline.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>

namespace {

struct particle {
  std::array<float, 4> position_size;
  std::array<float, 4> color;
};

constexpr std::uint32_t maximum_particles = 128;

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

class tutorial_application final : public granit::example::application {
public:
  void set_smoke_test(bool enabled) noexcept { smoke_test_ = enabled; }
  [[nodiscard]] std::uint32_t particle_count() const noexcept { return particle_count_; }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return runtime_.canvas_items(); }

private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.ok())
      result = frame_context_.initialize(renderer());
    if (result.ok())
      result = shader_library_.initialize(renderer(), tutorial_particles::shader_archive());
    if (result.ok())
      result = shader_library_.create_shader("particles.vertex", vertex_shader_);
    if (result.ok())
      result = shader_library_.create_shader("particles.fragment", fragment_shader_);
    if (result.ok())
      result = initialize_particles();
    if (result.ok())
      result = initialize_pipeline_layout();
    if (result.ok())
      result = create_pipeline();
    return result;
  }

  granit::result on_swapchain_changed(const granit::swapchain_info& info) noexcept override {
    return info.format == pipeline_format_ ? granit::result::success : create_pipeline();
  }

  void on_shutdown(granit::result) noexcept override {
    runtime_.shutdown();
    static_cast<void>(pipeline_.reset());
    static_cast<void>(resource_group_.reset());
    static_cast<void>(pipeline_layout_.reset());
    static_cast<void>(resource_layout_.reset());
    static_cast<void>(particles_.reset());
    static_cast<void>(fragment_shader_.reset());
    static_cast<void>(vertex_shader_.reset());
    static_cast<void>(shader_library_.reset());
    static_cast<void>(frame_context_.reset());
  }

  granit::result on_window_event(const granit::window_event& event) noexcept override {
    runtime_.process(event);
    return granit::result::success;
  }

  granit::result on_input_event(const granit::input_event& event) noexcept override {
    runtime_.process(event);
    return granit::result::success;
  }

  granit::result initialize_particles() noexcept {
    auto result = particles_.initialize(renderer_owner(),
                                        {.size = sizeof(particles_data_),
                                         .usage = granit::buffer_usage::storage |
                                                  granit::buffer_usage::transfer_destination});
    if (result.failed())
      return result;
    for (std::uint32_t index = 0; index < maximum_particles; ++index) {
      const float x = -0.9F + static_cast<float>(index % 16) * 0.12F;
      const float y = -0.7F + static_cast<float>(index / 16) * 0.20F;
      particles_data_[index] = {.position_size = {x, y, 0.025F, 0.0F},
                                 .color = {0.2F + static_cast<float>(index % 7) * 0.1F,
                                           0.35F + static_cast<float>(index % 5) * 0.1F, 1.0F,
                                           0.35F}};
    }
    return particles_.write(0, std::as_bytes(std::span{particles_data_}));
  }

  granit::result initialize_pipeline_layout() noexcept {
    const std::array entries{granit::bind_group_layout_entry{
        .binding = 0, .type = granit::binding_type::storage_buffer,
        .visibility = granit::shader_stage_flags::vertex}};
    auto result = resource_layout_.initialize(renderer_owner(), entries);
    const std::array layouts{resource_layout_.ref()};
    if (result.ok())
      result = pipeline_layout_.initialize(renderer_owner(), layouts);
    const std::array resources{granit::bind_group_entry{.binding = 0, .resource = particles_.ref(),
                                                        .size = sizeof(particles_data_)}};
    if (result.ok())
      result = resource_group_.initialize(renderer_owner(), resource_layout_, resources);
    return result;
  }

  granit::result create_pipeline() noexcept {
    auto result = pipeline_.reset();
    if (result.failed())
      return result;
    const auto format = presentation_info().format;
    const std::array blends{granit::color_blend_state{
        .source_color_factor = granit::blend_factor::source_alpha,
        .destination_color_factor = granit::blend_factor::one_minus_source_alpha,
        .source_alpha_factor = granit::blend_factor::one,
        .destination_alpha_factor = granit::blend_factor::one_minus_source_alpha}};
    result = pipeline_.initialize(renderer_owner(),
                                  {.layout = pipeline_layout_.ref(),
                                   .vertex_shader = vertex_shader_.ref(),
                                   .fragment_shader = fragment_shader_.ref(),
                                   .color_formats = std::span{&format, 1},
                                   .depth_stencil_format = granit::texture_format::undefined,
                                   .vertex_buffers = {}, .primitive = {}, .depth = std::nullopt,
                                   .color_blends = blends, .depth_bias = std::nullopt});
    if (result.ok())
      pipeline_format_ = format;
    return result;
  }

  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    granit::window_state window_state;
    auto result = app_window().get_state(window_state);
    if (result.ok())
      result = runtime_.begin_frame(window_state, frame.delta_seconds,
                                    {.name = "09 Particles", .description = "Dynamic storage data and alpha blending",
                                     .frame = rendered_frames()});
    if (result.ok()) {
      int count = static_cast<int>(particle_count_);
      ImGui::SliderInt("Particles", &count, 1, static_cast<int>(maximum_particles));
      ImGui::Checkbox("Animate", &animate_);
      ImGui::Text("Storage elements: %u", particle_count_);
      result = runtime_.end_frame();
    }
    if (result.failed())
      return result;
    granit::frame_recording recording;
    result = frame_context_.begin(frame.acquired, recording);
    particle_count_ = smoke_test_ ? 64U : particle_count_;
    if (animate_ || smoke_test_) {
      const float time = smoke_test_ ? 0.75F : elapsed_;
      for (std::uint32_t index = 0; index < particle_count_; ++index) {
        const float phase = time * 1.2F + static_cast<float>(index) * 0.23F;
        particles_data_[index].position_size[1] += std::sin(phase) * 0.0008F;
      }
      elapsed_ += frame.delta_seconds;
      if (result.ok())
        result = particles_.write(0, std::as_bytes(std::span{particles_data_}));
    }
    const granit::viewport viewport{0, 0, static_cast<float>(frame.swapchain.width),
                                   static_cast<float>(frame.swapchain.height), 0, 1};
    const granit::scissor scissor{0, 0, frame.swapchain.width, frame.swapchain.height};
    const granit::color_attachment_desc color{.view = frame.backbuffer.view, .resolve_view = {},
                                              .clear_value = {.red = 0.015F, .green = 0.02F,
                                                              .blue = 0.04F, .alpha = 1.0F}};
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                           .area = {0, 0, frame.swapchain.width,
                                                    frame.swapchain.height}};
    auto& recorder = recording.recorder();
    if (result.ok()) result = recorder.set_viewports(0, std::span{&viewport, 1});
    if (result.ok()) result = recorder.set_scissors(0, std::span{&scissor, 1});
    if (result.ok()) result = recorder.bind_graphics_pipeline(pipeline_);
    if (result.ok()) result = recorder.bind_graphics_group(pipeline_layout_, 0, resource_group_);
    if (result.ok()) result = recorder.begin_rendering(rendering);
    if (result.ok()) result = recorder.draw(particle_count_ * 6U);
    if (result.ok()) result = recorder.end_rendering();
    if (result.ok()) {
      const bool encode_srgb = frame.swapchain.format == granit::texture_format::rgba8_unorm ||
                               frame.swapchain.format == granit::texture_format::bgra8_unorm;
      result = runtime_.canvas().record(recorder, {.color = frame.backbuffer.view,
                                                   .color_format = frame.swapchain.format,
                                                   .width = frame.swapchain.width,
                                                   .height = frame.swapchain.height,
                                                   .load_operation = granit::attachment_load_operation::load,
                                                   .encode_srgb = encode_srgb,
                                                   .frame_slot = recording.frame_slot()});
    }
    if (result.ok()) result = recording.submit();
    if (result.failed() && recording.valid()) static_cast<void>(recording.abort());
    return result;
  }

  granit::example::tutorial::tutorial_runtime runtime_;
  granit::frame_context frame_context_;
  granit::shader_library shader_library_;
  granit::shader vertex_shader_, fragment_shader_;
  granit::buffer particles_;
  std::array<particle, maximum_particles> particles_data_{};
  granit::bind_group_layout resource_layout_;
  granit::pipeline_layout pipeline_layout_;
  granit::bind_group resource_group_;
  granit::graphics_pipeline pipeline_;
  granit::texture_format pipeline_format_{granit::texture_format::undefined};
  std::uint32_t particle_count_{maximum_particles};
  float elapsed_{};
  bool animate_{true};
  bool smoke_test_{};
};

tutorial_application application;
} // namespace

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";
  application.set_smoke_test(smoke_test);
  const auto executable_path = argument_count > 0 && arguments[0] != nullptr ? arguments[0] : "";
  const auto result = application.run({.executable_path = executable_path,
                                       .title = "Granit Particles",
                                       .renderer = {.application_name = "Granit Particles",
                                                    .presentation = granit::presentation_mode::enabled},
                                       .swapchain = {}, .smoke_test = smoke_test});
  if (smoke_test && result == granit::result::backend_unavailable) return 77;
  return result.failed() ? report_failure("application run", result) : 0;
}
