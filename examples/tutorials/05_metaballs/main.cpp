// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "camera/orbit_camera.h"
#include "camera/orbit_camera_input_accumulator.h"
#include "shader_archive.h"
#include "tutorial/tutorial_runtime.h"

#include <granit/math/functions.hpp>
#include <imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace {

struct metaballs_uniforms {
  std::array<float, 2> resolution;
  float time{};
  float surface_epsilon{};
  std::array<float, 4> camera_origin;
  std::array<float, 4> camera_forward_max_steps;
  std::array<float, 4> camera_right;
  std::array<float, 4> camera_up_tan_half_fov;
  std::array<float, 4> metaball_parameters;
};

static_assert(sizeof(metaballs_uniforms) == sizeof(float) * 24);

std::uint64_t align_up(std::uint64_t value, std::uint64_t alignment) {
  return alignment == 0 ? value : (value + alignment - 1) / alignment * alignment;
}

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

class tutorial_application final : public granit::example::application {
public:
  void set_smoke_test(bool enabled) noexcept { smoke_test_ = enabled; }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return runtime_.canvas_items(); }

private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.ok())
      result = frame_context_.initialize(renderer());
    if (result.ok())
      result = shader_library_.initialize(renderer(), tutorial_metaballs::shader_archive());
    if (result.ok())
      result = shader_library_.create_shader("metaballs.vertex", vertex_shader_);
    if (result.ok())
      result = shader_library_.create_shader("metaballs.fragment", fragment_shader_);
    granit::renderer_limits limits;
    if (result.ok())
      result = renderer_owner().get_limits(limits);
    if (result.ok()) {
      uniform_stride_ =
          align_up(sizeof(metaballs_uniforms), limits.uniform_buffer_offset_alignment);
      result = uniform_buffer_.initialize(
          renderer_owner(),
          {.size = uniform_stride_ * GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
           .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination});
    }
    if (result.ok())
      result = initialize_pipeline_layout();
    if (result.ok())
      result = create_pipeline();
    if (result.ok() &&
        !camera_.focus({.radius = 1.8F}, presentation_info().width, presentation_info().height))
      result = granit::result::invalid_argument;
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
    static_cast<void>(uniform_buffer_.reset());
    static_cast<void>(fragment_shader_.reset());
    static_cast<void>(vertex_shader_.reset());
    static_cast<void>(shader_library_.reset());
    static_cast<void>(frame_context_.reset());
  }

  granit::result on_window_event(const granit::window_event& event) noexcept override {
    runtime_.process(event);
    camera_input_.process(event);
    return granit::result::success;
  }

  granit::result on_input_event(const granit::input_event& event) noexcept override {
    runtime_.process(event);
    camera_input_.process(event, runtime_.wants_mouse(), runtime_.wants_keyboard());
    return granit::result::success;
  }

  granit::result initialize_pipeline_layout() noexcept {
    const std::array entries{
        granit::bind_group_layout_entry{.binding = 0,
                                        .type = granit::binding_type::dynamic_uniform_buffer,
                                        .visibility = granit::shader_stage_flags::fragment},
    };
    auto result = resource_layout_.initialize(renderer_owner(), entries);
    const std::array layouts{resource_layout_.ref()};
    if (result.ok())
      result = pipeline_layout_.initialize(renderer_owner(), layouts);
    const std::array resources{
        granit::bind_group_entry{
            .binding = 0, .resource = uniform_buffer_.ref(), .size = sizeof(metaballs_uniforms)},
    };
    if (result.ok())
      result = resource_group_.initialize(renderer_owner(), resource_layout_, resources);
    return result;
  }

  granit::result create_pipeline() noexcept {
    auto result = pipeline_.reset();
    if (result.failed())
      return result;
    const auto format = presentation_info().format;
    result = pipeline_.initialize(renderer_owner(),
                                  {.layout = pipeline_layout_.ref(),
                                   .vertex_shader = vertex_shader_.ref(),
                                   .fragment_shader = fragment_shader_.ref(),
                                   .color_formats = std::span{&format, 1},
                                   .depth_stencil_format = granit::texture_format::undefined,
                                   .samples = granit::sample_count::one,
                                   .vertex_buffers = {},
                                   .primitive = {},
                                   .depth = std::nullopt,
                                   .color_blends = {},
                                   .depth_bias = std::nullopt});
    if (result.ok())
      pipeline_format_ = format;
    return result;
  }

  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    camera_input_.begin_frame();
    granit::window_state window_state;
    auto result = app_window().get_state(window_state);
    if (result.ok()) {
      result = runtime_.begin_frame(window_state, frame.delta_seconds,
                                    {.name = "05 Metaballs",
                                     .description = "Animated SDF metaballs with smooth union",
                                     .frame = rendered_frames()});
    }
    if (result.ok()) {
      ImGui::SliderInt("Ball count", &ball_count_, 1, 5);
      ImGui::SliderFloat("Radius", &radius_, 0.15F, 0.9F);
      ImGui::SliderFloat("Smooth radius", &smooth_radius_, 0.02F, 0.8F);
      ImGui::Checkbox("Animate", &animate_);
      ImGui::SliderFloat("Animation speed", &animation_speed_, 0.0F, 3.0F);
      ImGui::Checkbox("Auto orbit", &auto_orbit_);
      if (ImGui::Button("Reset camera"))
        camera_.reset();
      result = runtime_.end_frame();
    }
    if (result.failed())
      return result;

    granit::frame_recording recording;
    result = frame_context_.begin(frame.acquired, recording);
    if (smoke_test_)
      time_ = 0.75F;
    else if (animate_)
      time_ += frame.delta_seconds * animation_speed_;

    const auto input = camera_input_.finish(runtime_.wants_mouse(), runtime_.wants_keyboard());
    if (auto_orbit_ && !smoke_test_ && !camera_.orbit(frame.delta_seconds * 0.25F))
      result = granit::result::invalid_argument;
    if (result.ok() && !camera_.update(input, frame.swapchain.width, frame.swapchain.height))
      result = granit::result::invalid_argument;
    granit::example::camera::camera_matrices matrices;
    if (result.ok() && !camera_.matrices(frame.swapchain.width, frame.swapchain.height, matrices))
      result = granit::result::invalid_argument;
    const auto forward =
        granit::math::normalize(granit::math::subtract(camera_.target(), matrices.position));
    const auto right = granit::math::normalize(granit::math::cross(forward, {0, 1, 0}));
    const auto up = granit::math::cross(right, forward);

    const metaballs_uniforms uniforms{
        .resolution = {static_cast<float>(frame.swapchain.width),
                       static_cast<float>(frame.swapchain.height)},
        .time = time_,
        .surface_epsilon = 0.001F,
        .camera_origin = {matrices.position.x, matrices.position.y, matrices.position.z, 0.0F},
        .camera_forward_max_steps = {forward.x, forward.y, forward.z, 96.0F},
        .camera_right = {right.x, right.y, right.z, 0.0F},
        .camera_up_tan_half_fov = {up.x, up.y, up.z, 0.414213562F},
        .metaball_parameters = {radius_, smooth_radius_, static_cast<float>(ball_count_), 0.0F},
    };
    const auto uniform_offset = uniform_stride_ * recording.frame_slot();
    if (result.ok())
      result = uniform_buffer_.write(uniform_offset, std::as_bytes(std::span{&uniforms, 1}));

    const granit::viewport viewport{
        0, 0, static_cast<float>(frame.swapchain.width), static_cast<float>(frame.swapchain.height),
        0, 1};
    const granit::scissor scissor{0, 0, frame.swapchain.width, frame.swapchain.height};
    const granit::color_attachment_desc color{
        .view = frame.backbuffer.view,
        .resolve_view = {},
        .clear_value = {.red = 0.01F, .green = 0.01F, .blue = 0.02F, .alpha = 1.0F}};
    const granit::rendering_desc rendering{
        .color_attachments = std::span{&color, 1},
        .depth_stencil_attachment = nullptr,
        .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
    auto& recorder = recording.recorder();
    if (result.ok())
      result = recorder.set_viewports(0, std::span{&viewport, 1});
    if (result.ok())
      result = recorder.set_scissors(0, std::span{&scissor, 1});
    if (result.ok())
      result = recorder.bind_graphics_pipeline(pipeline_);
    const std::array dynamic_offsets{static_cast<std::uint32_t>(uniform_offset)};
    if (result.ok())
      result = recorder.bind_graphics_group(pipeline_layout_, 0, resource_group_, dynamic_offsets);
    if (result.ok())
      result = recorder.begin_rendering(rendering);
    if (result.ok())
      result = recorder.draw(3);
    if (result.ok())
      result = recorder.end_rendering();
    if (result.ok()) {
      const bool encode_srgb = frame.swapchain.format == granit::texture_format::rgba8_unorm ||
                               frame.swapchain.format == granit::texture_format::bgra8_unorm;
      result = runtime_.canvas().record(recorder,
                                        {.color = frame.backbuffer.view,
                                         .color_format = frame.swapchain.format,
                                         .width = frame.swapchain.width,
                                         .height = frame.swapchain.height,
                                         .load_operation = granit::attachment_load_operation::load,
                                         .encode_srgb = encode_srgb,
                                         .frame_slot = recording.frame_slot()});
    }
    if (result.ok())
      result = recording.submit();
    if (result.failed() && recording.valid())
      static_cast<void>(recording.abort());
    return result;
  }

  granit::frame_context frame_context_;
  granit::example::tutorial::tutorial_runtime runtime_;
  granit::example::camera::orbit_camera camera_;
  granit::example::camera::orbit_camera_input_accumulator camera_input_;
  granit::shader_library shader_library_;
  granit::shader vertex_shader_;
  granit::shader fragment_shader_;
  granit::buffer uniform_buffer_;
  granit::bind_group_layout resource_layout_;
  granit::pipeline_layout pipeline_layout_;
  granit::bind_group resource_group_;
  granit::graphics_pipeline pipeline_;
  granit::texture_format pipeline_format_{granit::texture_format::undefined};
  std::uint64_t uniform_stride_{};
  float time_{};
  float radius_{0.42F};
  float smooth_radius_{0.38F};
  float animation_speed_{1.0F};
  int ball_count_{5};
  bool animate_{true};
  bool auto_orbit_{};
  bool smoke_test_{};
};

tutorial_application application;

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_05_rendered_frames() noexcept {
  return application.rendered_frames();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_05_recreate_count() noexcept {
  return application.completed_recreates();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_05_feature_value() noexcept {
  return 5;
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_05_canvas_items() noexcept {
  return application.canvas_items();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_05_ready() noexcept {
  return application.ready() ? 1 : 0;
}
#endif

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";
  const std::string_view executable_path =
      argument_count > 0 && arguments[0] != nullptr ? arguments[0] : "";
  application.set_smoke_test(smoke_test);

  const auto result =
      application.run({.executable_path = executable_path,
                       .title = "Granit Metaballs",
                       .renderer = {.application_name = "Granit Metaballs",
                                    .presentation = granit::presentation_mode::enabled},
                       .swapchain = {},
                       .smoke_test = smoke_test});
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  if (result.failed())
    return report_failure("application run", result);
  return 0;
}
