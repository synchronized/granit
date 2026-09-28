// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "camera/orbit_camera.h"
#include "camera/orbit_camera_input_accumulator.h"
#include "model_data.hpp"
#include "shader_archive.h"
#include "tutorial/tutorial_runtime.h"

#include <granit/pipeline/mesh.hpp>
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

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace {

using granit::math::matrix4;

constexpr std::uint32_t maximum_light_count = 64;
constexpr std::uint32_t instance_rows = 5;
constexpr std::uint32_t instance_columns = 7;
constexpr std::uint32_t instance_count = instance_rows * instance_columns;

enum class output_mode : std::uint32_t { final_lighting, position_depth, normal, albedo };

struct instance_data {
  std::array<float, 4> offset_scale;
  std::array<float, 4> color;
};

struct geometry_uniforms {
  matrix4 view;
  matrix4 view_projection;
  std::array<float, 4> animation{};
};

struct point_light {
  std::array<float, 4> position_radius;
  std::array<float, 4> color_intensity;
};

struct lighting_uniforms {
  std::array<float, 2> resolution;
  float exposure{};
  std::uint32_t mode{};
  float ambient{};
  float time{};
  std::uint32_t light_count{};
  float padding{};
  std::array<point_light, maximum_light_count> lights;
};

static_assert(sizeof(instance_data) == sizeof(float) * 8);
static_assert(sizeof(geometry_uniforms) == sizeof(float) * 36);
static_assert(sizeof(point_light) == sizeof(float) * 8);

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
  void set_output_mode(std::uint32_t value) noexcept {
    if (value <= static_cast<std::uint32_t>(output_mode::albedo))
      mode_ = static_cast<output_mode>(value);
  }
  [[nodiscard]] std::uint32_t feature_value() const noexcept {
    return (light_count_ << 8U) | (static_cast<std::uint32_t>(mode_) + 1U);
  }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return runtime_.canvas_items(); }

private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.ok())
      result = validate_capabilities();
    if (result.ok())
      result = frame_context_.initialize(renderer());
    if (result.ok())
      result = shader_library_.initialize(renderer(), tutorial_deferred::shader_archive());
    if (result.ok())
      result = shader_library_.create_shader("deferred.geometry_vertex", geometry_vertex_shader_);
    if (result.ok()) {
      result =
          shader_library_.create_shader("deferred.geometry_fragment", geometry_fragment_shader_);
    }
    if (result.ok())
      result = shader_library_.create_shader("deferred.lighting_vertex", lighting_vertex_shader_);
    if (result.ok()) {
      result =
          shader_library_.create_shader("deferred.lighting_fragment", lighting_fragment_shader_);
    }
    if (result.ok())
      result = initialize_buffers();
    if (result.ok())
      result = gbuffer_sampler_.initialize(renderer_owner(),
                                           {.mag_filter = granit::filter::nearest,
                                            .min_filter = granit::filter::nearest,
                                            .mip_filter = granit::mipmap_filter::nearest,
                                            .address_u = granit::address_mode::clamp_to_edge,
                                            .address_v = granit::address_mode::clamp_to_edge,
                                            .address_w = granit::address_mode::clamp_to_edge});
    if (result.ok())
      result = initialize_pipeline_layouts();
    if (result.ok())
      result = create_gbuffer();
    if (result.ok())
      result = create_geometry_pipeline();
    if (result.ok())
      result = create_lighting_pipeline();
    if (result.ok() &&
        !camera_.focus({.radius = 7.0F}, presentation_info().width, presentation_info().height))
      result = granit::result::invalid_argument;
    return result;
  }

  granit::result on_swapchain_changed(const granit::swapchain_info& info) noexcept override {
    auto result = create_gbuffer();
    if (result.ok() && info.format != lighting_pipeline_format_)
      result = create_lighting_pipeline();
    return result;
  }

  void on_shutdown(granit::result) noexcept override {
    runtime_.shutdown();
    static_cast<void>(lighting_pipeline_.reset());
    static_cast<void>(geometry_pipeline_.reset());
    static_cast<void>(lighting_group_.reset());
    static_cast<void>(geometry_group_.reset());
    static_cast<void>(lighting_pipeline_layout_.reset());
    static_cast<void>(geometry_pipeline_layout_.reset());
    static_cast<void>(lighting_layout_.reset());
    static_cast<void>(geometry_layout_.reset());
    static_cast<void>(reset_gbuffer());
    static_cast<void>(gbuffer_sampler_.reset());
    static_cast<void>(lighting_uniform_buffer_.reset());
    static_cast<void>(geometry_uniform_buffer_.reset());
    static_cast<void>(mesh_.reset());
    static_cast<void>(instance_buffer_.reset());
    static_cast<void>(index_buffer_.reset());
    static_cast<void>(vertex_buffer_.reset());
    static_cast<void>(lighting_fragment_shader_.reset());
    static_cast<void>(lighting_vertex_shader_.reset());
    static_cast<void>(geometry_fragment_shader_.reset());
    static_cast<void>(geometry_vertex_shader_.reset());
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

  granit::result validate_capabilities() noexcept {
    granit::renderer_limits limits;
    auto result = renderer_owner().get_limits(limits);
    if (result.failed() || limits.max_color_attachments < 3 ||
        limits.max_uniform_buffer_binding_size < sizeof(lighting_uniforms))
      return result.failed() ? result : granit::result::unsupported;
    constexpr auto color_usage =
        granit::texture_usage::color_attachment | granit::texture_usage::sampled;
    for (const auto format :
         {granit::texture_format::rgba16_float, granit::texture_format::rgba8_unorm}) {
      granit::texture_format_capabilities capabilities;
      result = granit::get_texture_format_capabilities(renderer_owner(), format, capabilities);
      if (result.failed() || !capabilities.supports(color_usage))
        return result.failed() ? result : granit::result::unsupported;
    }
    granit::texture_format_capabilities depth_capabilities;
    result = granit::get_texture_format_capabilities(
        renderer_owner(), granit::texture_format::d32_float, depth_capabilities);
    if (result.failed() ||
        !depth_capabilities.supports(granit::texture_usage::depth_stencil_attachment))
      return result.failed() ? result : granit::result::unsupported;
    return granit::result::success;
  }

  void initialize_instances() noexcept {
    for (std::uint32_t row = 0; row < instance_rows; ++row) {
      for (std::uint32_t column = 0; column < instance_columns; ++column) {
        const auto index = row * instance_columns + column;
        const float x = (static_cast<float>(column) - 3.0F) * 1.55F;
        const float y = (static_cast<float>(row) - 2.0F) * 1.55F;
        const float z = static_cast<float>((row + column) % 3U) * -0.75F;
        instances_[index] = {
            .offset_scale = {x, y, z, 0.55F},
            .color = {0.2F + static_cast<float>(column) * 0.1F,
                      0.25F + static_cast<float>(row) * 0.16F,
                      0.9F - static_cast<float>(column + row) * 0.06F, 1.0F},
        };
      }
    }
  }

  granit::result initialize_buffers() noexcept {
    initialize_instances();
    auto result = vertex_buffer_.initialize(
        renderer_owner(),
        {.size = sizeof(tutorial_deferred::vertices),
         .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination});
    if (result.ok()) {
      result = index_buffer_.initialize(
          renderer_owner(),
          {.size = sizeof(tutorial_deferred::indices),
           .usage = granit::buffer_usage::index | granit::buffer_usage::transfer_destination});
    }
    if (result.ok()) {
      result = instance_buffer_.initialize(
          renderer_owner(),
          {.size = sizeof(instances_),
           .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination});
    }
    granit::renderer_limits limits;
    if (result.ok())
      result = renderer_owner().get_limits(limits);
    if (result.ok()) {
      geometry_uniform_stride_ =
          align_up(sizeof(geometry_uniforms), limits.uniform_buffer_offset_alignment);
      lighting_uniform_stride_ =
          align_up(sizeof(lighting_uniforms), limits.uniform_buffer_offset_alignment);
      result = geometry_uniform_buffer_.initialize(
          renderer_owner(),
          {.size = geometry_uniform_stride_ * GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
           .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination});
    }
    if (result.ok()) {
      result = lighting_uniform_buffer_.initialize(
          renderer_owner(),
          {.size = lighting_uniform_stride_ * GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
           .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination});
    }
    granit::upload_batch upload;
    if (result.ok())
      result = upload.initialize(renderer_owner());
    if (result.ok()) {
      result = upload.write_buffer(vertex_buffer_.ref(), 0,
                                   std::as_bytes(std::span{tutorial_deferred::vertices}));
    }
    if (result.ok()) {
      result = upload.write_buffer(index_buffer_.ref(), 0,
                                   std::as_bytes(std::span{tutorial_deferred::indices}));
    }
    if (result.ok())
      result = upload.write_buffer(instance_buffer_.ref(), 0, std::as_bytes(std::span{instances_}));
    if (result.ok())
      result = upload.submit();
    if (result.ok())
      result = initialize_mesh();
    return result;
  }

  granit::result initialize_mesh() noexcept {
    const std::array vertex_attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{
            .location = 1, .format = granit::vertex_format::float32x3, .offset = sizeof(float) * 3},
    };
    const std::array instance_attributes{
        granit::vertex_attribute{.location = 2, .format = granit::vertex_format::float32x4},
        granit::vertex_attribute{
            .location = 3, .format = granit::vertex_format::float32x4, .offset = sizeof(float) * 4},
    };
    const std::array bindings{
        granit::mesh_vertex_buffer{.buffer = vertex_buffer_.ref(),
                                   .layout = {.stride = sizeof(tutorial_deferred::vertex),
                                              .attributes = vertex_attributes}},
        granit::mesh_vertex_buffer{.buffer = instance_buffer_.ref(),
                                   .layout = {.stride = sizeof(instance_data),
                                              .step_mode = granit::vertex_step_mode::instance,
                                              .attributes = instance_attributes}},
    };
    return mesh_.initialize(renderer_owner(), {.vertex_buffers = bindings,
                                               .index_buffer = index_buffer_.ref(),
                                               .index_format = granit::index_type::uint16,
                                               .index_count = static_cast<std::uint32_t>(
                                                   tutorial_deferred::indices.size()),
                                               .instance_count = instance_count});
  }

  granit::result initialize_pipeline_layouts() noexcept {
    const std::array geometry_entries{
        granit::bind_group_layout_entry{.binding = 0,
                                        .type = granit::binding_type::dynamic_uniform_buffer,
                                        .visibility = granit::shader_stage_flags::vertex}};
    auto result = geometry_layout_.initialize(renderer_owner(), geometry_entries);
    const std::array geometry_layouts{geometry_layout_.ref()};
    if (result.ok())
      result = geometry_pipeline_layout_.initialize(renderer_owner(), geometry_layouts);
    const std::array geometry_resources{
        granit::bind_group_entry{.binding = 0,
                                 .resource = geometry_uniform_buffer_.ref(),
                                 .size = sizeof(geometry_uniforms)}};
    if (result.ok()) {
      result = geometry_group_.initialize(renderer_owner(), geometry_layout_, geometry_resources);
    }

    const std::array lighting_entries{
        granit::bind_group_layout_entry{.binding = 0,
                                        .type = granit::binding_type::dynamic_uniform_buffer,
                                        .visibility = granit::shader_stage_flags::fragment},
        granit::bind_group_layout_entry{.binding = 1,
                                        .type = granit::binding_type::sampled_texture,
                                        .visibility = granit::shader_stage_flags::fragment},
        granit::bind_group_layout_entry{.binding = 2,
                                        .type = granit::binding_type::sampled_texture,
                                        .visibility = granit::shader_stage_flags::fragment},
        granit::bind_group_layout_entry{.binding = 3,
                                        .type = granit::binding_type::sampled_texture,
                                        .visibility = granit::shader_stage_flags::fragment},
        granit::bind_group_layout_entry{.binding = 4,
                                        .type = granit::binding_type::sampler,
                                        .visibility = granit::shader_stage_flags::fragment},
    };
    if (result.ok())
      result = lighting_layout_.initialize(renderer_owner(), lighting_entries);
    const std::array lighting_layouts{lighting_layout_.ref()};
    if (result.ok())
      result = lighting_pipeline_layout_.initialize(renderer_owner(), lighting_layouts);
    return result;
  }

  granit::result reset_gbuffer() noexcept {
    auto result = lighting_group_.reset();
    if (result.ok())
      result = depth_view_.reset();
    if (result.ok())
      result = albedo_view_.reset();
    if (result.ok())
      result = normal_view_.reset();
    if (result.ok())
      result = position_view_.reset();
    if (result.ok())
      result = depth_texture_.reset();
    if (result.ok())
      result = albedo_texture_.reset();
    if (result.ok())
      result = normal_texture_.reset();
    if (result.ok())
      result = position_texture_.reset();
    return result;
  }

  granit::result create_gbuffer() noexcept {
    auto result = reset_gbuffer();
    if (result.failed())
      return result;
    const auto width = presentation_info().width;
    const auto height = presentation_info().height;
    if (width == 0 || height == 0)
      return granit::result::success;
    constexpr auto usage = granit::texture_usage::color_attachment | granit::texture_usage::sampled;
    result = position_texture_.initialize(renderer_owner(),
                                          {.format = granit::texture_format::rgba16_float,
                                           .usage = usage,
                                           .width = width,
                                           .height = height});
    if (result.ok()) {
      result = normal_texture_.initialize(renderer_owner(),
                                          {.format = granit::texture_format::rgba16_float,
                                           .usage = usage,
                                           .width = width,
                                           .height = height});
    }
    if (result.ok()) {
      result = albedo_texture_.initialize(renderer_owner(),
                                          {.format = granit::texture_format::rgba8_unorm,
                                           .usage = usage,
                                           .width = width,
                                           .height = height});
    }
    if (result.ok()) {
      result = depth_texture_.initialize(renderer_owner(),
                                         {.format = granit::texture_format::d32_float,
                                          .usage = granit::texture_usage::depth_stencil_attachment,
                                          .width = width,
                                          .height = height});
    }
    if (result.ok())
      result = position_view_.initialize(renderer_owner(), position_texture_);
    if (result.ok())
      result = normal_view_.initialize(renderer_owner(), normal_texture_);
    if (result.ok())
      result = albedo_view_.initialize(renderer_owner(), albedo_texture_);
    if (result.ok())
      result = depth_view_.initialize(renderer_owner(), depth_texture_);
    const std::array resources{
        granit::bind_group_entry{.binding = 0,
                                 .resource = lighting_uniform_buffer_.ref(),
                                 .size = sizeof(lighting_uniforms)},
        granit::bind_group_entry{.binding = 1, .resource = position_view_.ref()},
        granit::bind_group_entry{.binding = 2, .resource = normal_view_.ref()},
        granit::bind_group_entry{.binding = 3, .resource = albedo_view_.ref()},
        granit::bind_group_entry{.binding = 4, .resource = gbuffer_sampler_.ref()},
    };
    if (result.ok())
      result = lighting_group_.initialize(renderer_owner(), lighting_layout_, resources);
    if (result.failed())
      static_cast<void>(reset_gbuffer());
    return result;
  }

  granit::result create_geometry_pipeline() noexcept {
    const std::array formats{granit::texture_format::rgba16_float,
                             granit::texture_format::rgba16_float,
                             granit::texture_format::rgba8_unorm};
    const std::array vertex_attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{
            .location = 1, .format = granit::vertex_format::float32x3, .offset = sizeof(float) * 3},
    };
    const std::array instance_attributes{
        granit::vertex_attribute{.location = 2, .format = granit::vertex_format::float32x4},
        granit::vertex_attribute{
            .location = 3, .format = granit::vertex_format::float32x4, .offset = sizeof(float) * 4},
    };
    const std::array vertex_layouts{
        granit::vertex_buffer_layout{.stride = sizeof(tutorial_deferred::vertex),
                                     .attributes = vertex_attributes},
        granit::vertex_buffer_layout{.stride = sizeof(instance_data),
                                     .step_mode = granit::vertex_step_mode::instance,
                                     .attributes = instance_attributes},
    };
    return geometry_pipeline_.initialize(
        renderer_owner(),
        {.layout = geometry_pipeline_layout_.ref(),
         .vertex_shader = geometry_vertex_shader_.ref(),
         .fragment_shader = geometry_fragment_shader_.ref(),
         .color_formats = formats,
         .depth_stencil_format = granit::texture_format::d32_float,
         .vertex_buffers = vertex_layouts,
         .primitive = {.cull = granit::cull_mode::back},
         .depth = granit::depth_state{.test_enabled = true, .write_enabled = true},
         .color_blends = {},
         .depth_bias = std::nullopt});
  }

  granit::result create_lighting_pipeline() noexcept {
    auto result = lighting_pipeline_.reset();
    if (result.failed())
      return result;
    const auto format = presentation_info().format;
    result = lighting_pipeline_.initialize(
        renderer_owner(), {.layout = lighting_pipeline_layout_.ref(),
                           .vertex_shader = lighting_vertex_shader_.ref(),
                           .fragment_shader = lighting_fragment_shader_.ref(),
                           .color_formats = std::span{&format, 1},
                           .depth_stencil_format = granit::texture_format::undefined,
                           .vertex_buffers = {},
                           .primitive = {},
                           .depth = std::nullopt,
                           .color_blends = {},
                           .depth_bias = std::nullopt});
    if (result.ok())
      lighting_pipeline_format_ = format;
    return result;
  }

  void update_lights(const matrix4& view, lighting_uniforms& uniforms) const noexcept {
    const std::array colors{
        std::array<float, 3>{1.0F, 0.25F, 0.18F}, std::array<float, 3>{0.2F, 0.55F, 1.0F},
        std::array<float, 3>{0.3F, 1.0F, 0.45F}, std::array<float, 3>{1.0F, 0.75F, 0.22F}};
    for (std::uint32_t index = 0; index < light_count_; ++index) {
      const float normalized = static_cast<float>(index) / static_cast<float>(light_count_);
      const float angle = normalized * 6.283185307F + time_ * 0.35F;
      const float ring = 2.5F + static_cast<float>(index % 5U) * 0.7F;
      const granit::math::float3 world{std::cos(angle) * ring, std::sin(angle * 1.7F) * 2.8F,
                                       std::sin(angle) * ring - 1.0F};
      granit::math::float3 position{};
      static_cast<void>(granit::math::transform_point(view, world, position));
      const auto& color = colors[index % colors.size()];
      uniforms.lights[index] = {
          .position_radius = {position.x, position.y, position.z, 5.5F},
          .color_intensity = {color[0], color[1], color[2], 2.2F},
      };
    }
  }

  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    camera_input_.begin_frame();
    granit::window_state window_state;
    auto result = app_window().get_state(window_state);
    if (result.ok()) {
      result = runtime_.begin_frame(window_state, frame.delta_seconds,
                                    {.name = "07 Deferred",
                                     .description = "MRT G-buffer and multi-pass lighting",
                                     .frame = rendered_frames()});
    }
    int mode = static_cast<int>(mode_);
    int lights = static_cast<int>(light_count_);
    if (result.ok()) {
      constexpr std::array labels{"Final lighting", "Position / depth", "Normal", "Albedo"};
      ImGui::Combo("Output", &mode, labels.data(), static_cast<int>(labels.size()));
      ImGui::SliderInt("Point lights", &lights, 1, static_cast<int>(maximum_light_count));
      ImGui::SliderFloat("Exposure", &exposure_, 0.1F, 3.0F);
      ImGui::SliderFloat("Ambient", &ambient_, 0.0F, 0.4F);
      ImGui::Checkbox("Animate", &animate_);
      ImGui::SliderFloat("Animation speed", &animation_speed_, 0.0F, 3.0F);
      ImGui::Checkbox("Auto orbit", &auto_orbit_);
      if (ImGui::Button("Reset camera"))
        camera_.reset();
      ImGui::Text("G-buffer: %u x %u, 3 color attachments", frame.swapchain.width,
                  frame.swapchain.height);
      ImGui::Text("Passes: Geometry -> Lighting -> Canvas");
      result = runtime_.end_frame();
    }
    if (result.failed())
      return result;
    mode_ = static_cast<output_mode>(mode);
    light_count_ = static_cast<std::uint32_t>(lights);

    granit::frame_recording recording;
    result = frame_context_.begin(frame.acquired, recording);
    if (smoke_test_) {
      time_ = 0.75F;
      light_count_ = 12;
      mode_ = static_cast<output_mode>(rendered_frames() % 4U);
    } else if (animate_) {
      time_ += frame.delta_seconds * animation_speed_;
    }
    const auto input = camera_input_.finish(runtime_.wants_mouse(), runtime_.wants_keyboard());
    if (auto_orbit_ && !smoke_test_ && !camera_.orbit(frame.delta_seconds * 0.2F))
      result = granit::result::invalid_argument;
    if (result.ok() && !camera_.update(input, frame.swapchain.width, frame.swapchain.height))
      result = granit::result::invalid_argument;
    granit::example::camera::camera_matrices matrices;
    if (result.ok() && !camera_.matrices(frame.swapchain.width, frame.swapchain.height, matrices))
      result = granit::result::invalid_argument;

    geometry_uniforms geometry{};
    geometry.view = matrices.view;
    geometry.view_projection = matrices.view_projection;
    geometry.animation[0] = time_;
    lighting_uniforms lighting{};
    lighting.resolution = {static_cast<float>(frame.swapchain.width),
                           static_cast<float>(frame.swapchain.height)};
    lighting.exposure = exposure_;
    lighting.mode = static_cast<std::uint32_t>(mode_);
    lighting.ambient = ambient_;
    lighting.time = time_;
    lighting.light_count = light_count_;
    update_lights(matrices.view, lighting);
    const auto geometry_offset = geometry_uniform_stride_ * recording.frame_slot();
    const auto lighting_offset = lighting_uniform_stride_ * recording.frame_slot();
    if (result.ok()) {
      result =
          geometry_uniform_buffer_.write(geometry_offset, std::as_bytes(std::span{&geometry, 1}));
    }
    if (result.ok()) {
      result =
          lighting_uniform_buffer_.write(lighting_offset, std::as_bytes(std::span{&lighting, 1}));
    }

    const granit::viewport viewport{
        0, 0, static_cast<float>(frame.swapchain.width), static_cast<float>(frame.swapchain.height),
        0, 1};
    const granit::scissor scissor{0, 0, frame.swapchain.width, frame.swapchain.height};
    const std::array gbuffer_colors{
        granit::color_attachment_desc{
            .view = position_view_.ref(), .resolve_view = {}, .clear_value = {.alpha = 0.0F}},
        granit::color_attachment_desc{
            .view = normal_view_.ref(), .resolve_view = {}, .clear_value = {.alpha = 0.0F}},
        granit::color_attachment_desc{
            .view = albedo_view_.ref(), .resolve_view = {}, .clear_value = {.alpha = 0.0F}},
    };
    const granit::depth_stencil_attachment_desc depth{.view = depth_view_.ref(),
                                                      .clear_value = {.depth = 1.0F}};
    const granit::rendering_desc geometry_rendering{
        .color_attachments = gbuffer_colors,
        .depth_stencil_attachment = &depth,
        .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
    const granit::color_attachment_desc final_color{
        .view = frame.backbuffer.view,
        .resolve_view = {},
        .clear_value = {.red = 0.018F, .green = 0.025F, .blue = 0.05F, .alpha = 1.0F}};
    const granit::rendering_desc lighting_rendering{
        .color_attachments = std::span{&final_color, 1},
        .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
    auto& recorder = recording.recorder();
    if (result.ok())
      result = recorder.set_viewports(0, std::span{&viewport, 1});
    if (result.ok())
      result = recorder.set_scissors(0, std::span{&scissor, 1});
    if (result.ok())
      result = recorder.bind_graphics_pipeline(geometry_pipeline_);
    if (result.ok())
      result = mesh_.bind(recorder);
    const std::array geometry_offsets{static_cast<std::uint32_t>(geometry_offset)};
    if (result.ok()) {
      result = recorder.bind_graphics_group(geometry_pipeline_layout_, 0, geometry_group_,
                                            geometry_offsets);
    }
    if (result.ok())
      result = recorder.begin_rendering(geometry_rendering);
    if (result.ok())
      result = mesh_.draw(recorder, {.instance_count = instance_count});
    if (result.ok())
      result = recorder.end_rendering();

    if (result.ok())
      result = recorder.bind_graphics_pipeline(lighting_pipeline_);
    const std::array lighting_offsets{static_cast<std::uint32_t>(lighting_offset)};
    if (result.ok()) {
      result = recorder.bind_graphics_group(lighting_pipeline_layout_, 0, lighting_group_,
                                            lighting_offsets);
    }
    if (result.ok())
      result = recorder.begin_rendering(lighting_rendering);
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

  granit::example::tutorial::tutorial_runtime runtime_;
  granit::example::camera::orbit_camera camera_;
  granit::example::camera::orbit_camera_input_accumulator camera_input_;
  granit::frame_context frame_context_;
  granit::shader_library shader_library_;
  granit::shader geometry_vertex_shader_;
  granit::shader geometry_fragment_shader_;
  granit::shader lighting_vertex_shader_;
  granit::shader lighting_fragment_shader_;
  granit::buffer vertex_buffer_;
  granit::buffer index_buffer_;
  granit::buffer instance_buffer_;
  granit::mesh mesh_;
  granit::buffer geometry_uniform_buffer_;
  granit::buffer lighting_uniform_buffer_;
  granit::sampler gbuffer_sampler_;
  granit::texture position_texture_;
  granit::texture normal_texture_;
  granit::texture albedo_texture_;
  granit::texture depth_texture_;
  granit::texture_view position_view_;
  granit::texture_view normal_view_;
  granit::texture_view albedo_view_;
  granit::texture_view depth_view_;
  granit::bind_group_layout geometry_layout_;
  granit::bind_group_layout lighting_layout_;
  granit::pipeline_layout geometry_pipeline_layout_;
  granit::pipeline_layout lighting_pipeline_layout_;
  granit::bind_group geometry_group_;
  granit::bind_group lighting_group_;
  granit::graphics_pipeline geometry_pipeline_;
  granit::graphics_pipeline lighting_pipeline_;
  std::array<instance_data, instance_count> instances_{};
  granit::texture_format lighting_pipeline_format_{granit::texture_format::undefined};
  std::uint64_t geometry_uniform_stride_{};
  std::uint64_t lighting_uniform_stride_{};
  float time_{};
  float exposure_{1.0F};
  float ambient_{0.08F};
  float animation_speed_{1.0F};
  std::uint32_t light_count_{12};
  output_mode mode_{output_mode::final_lighting};
  bool animate_{true};
  bool auto_orbit_{};
  bool smoke_test_{};
};

tutorial_application application;

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_07_rendered_frames() noexcept {
  return application.rendered_frames();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_07_recreate_count() noexcept {
  return application.completed_recreates();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_07_feature_value() noexcept {
  return application.feature_value();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_07_canvas_items() noexcept {
  return application.canvas_items();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_07_ready() noexcept {
  return application.ready() ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_07_pass_count() noexcept { return 3; }

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_07_light_count() noexcept {
  return application.feature_value() >> 8U;
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_07_output_mode() noexcept {
  return (application.feature_value() & 0xffU) - 1U;
}

extern "C" EMSCRIPTEN_KEEPALIVE void
granit_tutorial_07_set_output_mode(std::uint32_t value) noexcept {
  application.set_output_mode(value);
}
#endif

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";
  const std::string_view executable_path =
      argument_count > 0 && arguments[0] != nullptr ? arguments[0] : "";
  application.set_smoke_test(smoke_test);
  const auto result =
      application.run({.executable_path = executable_path,
                       .title = "Granit Deferred",
                       .renderer = {.application_name = "Granit Deferred",
                                    .presentation = granit::presentation_mode::enabled},
                       .swapchain = {},
                       .smoke_test = smoke_test});
  if (smoke_test &&
      (result == granit::result::backend_unavailable || result == granit::result::unsupported))
    return 77;
  if (result.failed())
    return report_failure("application run", result);
  return 0;
}
