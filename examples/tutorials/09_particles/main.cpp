// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "camera/orbit_camera.h"
#include "camera/orbit_camera_input_accumulator.h"
#include "model_data.hpp"
#include "shader_archive.h"
#include "tutorial/tutorial_runtime.h"

#include <granit/math/types.hpp>
#include <granit/pipeline/debug_draw_list.hpp>
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

constexpr std::uint32_t maximum_particles = 256;
constexpr std::uint32_t smoke_particles = 64;
constexpr std::uint32_t sphere_shape = 0;
constexpr std::uint32_t cube_shape = 1;
constexpr std::uint32_t box_shape = 2;
constexpr std::uint32_t continuous_emission = 1;
constexpr float pi = 3.14159265F;
constexpr std::array<float, 3> emitter_position{0.0F, -2.2F, 0.0F};
constexpr std::array<float, 3> light_plane_position{0.0F, 2.5F, 0.0F};
constexpr std::array<float, 3> light_travel_direction{0.0F, -1.0F, 0.0F};
constexpr std::array<float, 3> light_direction{0.0F, 1.0F, 0.0F};

struct particle_state {
  std::array<float, 4> position{};
  std::array<float, 4> velocity{};
  std::array<float, 4> color{};
  float lifetime{};
  float age{};
  std::array<float, 3> rotation{};
  std::array<float, 3> angular_velocity{};
};

struct particle_instance {
  std::array<float, 4> position_scale{};
  std::array<float, 4> color{};
  std::array<float, 4> shape_scale{};
  std::array<float, 4> rotation{};
};

struct scene_uniforms {
  matrix4 view_projection;
  std::array<float, 4> light_direction;
  std::array<float, 4> ambient;
  std::array<float, 4> debug;
};

static_assert(sizeof(particle_instance) == sizeof(float) * 16);
static_assert(sizeof(scene_uniforms) == sizeof(float) * 28);

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

constexpr std::uint32_t rgba8(std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                              std::uint8_t alpha = 255) noexcept {
  return static_cast<std::uint32_t>(red) | (static_cast<std::uint32_t>(green) << 8U) |
         (static_cast<std::uint32_t>(blue) << 16U) | (static_cast<std::uint32_t>(alpha) << 24U);
}

void append_gizmo_line(std::array<granit::debug_draw_line, 128>& lines, std::size_t& count,
                       const std::array<float, 3>& start, const std::array<float, 3>& end,
                       std::uint32_t color, float width = 2.0F) noexcept {
  if (count >= lines.size())
    return;
  lines[count++] = {
      .start = {.x = start[0], .y = start[1], .z = start[2], .color = color},
      .end = {.x = end[0], .y = end[1], .z = end[2], .color = color},
      .width = width,
      .space = granit::debug_draw_space::world,
      .depth_mode = granit::debug_draw_depth_mode::test,
  };
}

void append_gizmo_sphere(std::array<granit::debug_draw_line, 128>& lines, std::size_t& count,
                         const std::array<float, 3>& center, float radius,
                         std::uint32_t color) noexcept {
  constexpr std::size_t segments = 16;
  for (std::size_t segment = 0; segment < segments; ++segment) {
    const auto first = static_cast<float>(segment) / static_cast<float>(segments) * 2.0F * pi;
    const auto second = static_cast<float>(segment + 1) / static_cast<float>(segments) * 2.0F * pi;
    append_gizmo_line(lines, count,
                      {center[0] + std::cos(first) * radius, center[1] + std::sin(first) * radius,
                       center[2]},
                      {center[0] + std::cos(second) * radius,
                       center[1] + std::sin(second) * radius, center[2]},
                      color);
    append_gizmo_line(lines, count,
                      {center[0] + std::cos(first) * radius, center[1],
                       center[2] + std::sin(first) * radius},
                      {center[0] + std::cos(second) * radius, center[1],
                       center[2] + std::sin(second) * radius},
                      color);
    append_gizmo_line(lines, count,
                      {center[0], center[1] + std::cos(first) * radius,
                       center[2] + std::sin(first) * radius},
                      {center[0], center[1] + std::cos(second) * radius,
                       center[2] + std::sin(second) * radius},
                      color);
  }
}

void append_gizmo_axes(std::array<granit::debug_draw_line, 128>& lines, std::size_t& count,
                       const std::array<float, 3>& center, float size) noexcept {
  append_gizmo_line(lines, count, center,
                    {center[0] + size, center[1], center[2]}, rgba8(240, 64, 64), 2.5F);
  append_gizmo_line(lines, count, center,
                    {center[0], center[1] + size, center[2]}, rgba8(64, 240, 96), 2.5F);
  append_gizmo_line(lines, count, center,
                    {center[0], center[1], center[2] + size}, rgba8(64, 128, 255), 2.5F);
}

void append_gizmo_square(std::array<granit::debug_draw_line, 128>& lines, std::size_t& count,
                         const std::array<float, 3>& center, float half_size,
                         std::uint32_t color) noexcept {
  const std::array<float, 3> top_left{center[0] - half_size, center[1], center[2] - half_size};
  const std::array<float, 3> top_right{center[0] + half_size, center[1], center[2] - half_size};
  const std::array<float, 3> bottom_right{center[0] + half_size, center[1],
                                          center[2] + half_size};
  const std::array<float, 3> bottom_left{center[0] - half_size, center[1],
                                         center[2] + half_size};
  append_gizmo_line(lines, count, top_left, top_right, color, 2.5F);
  append_gizmo_line(lines, count, top_right, bottom_right, color, 2.5F);
  append_gizmo_line(lines, count, bottom_right, bottom_left, color, 2.5F);
  append_gizmo_line(lines, count, bottom_left, top_left, color, 2.5F);
}

std::array<float, 3> normalized_or_up(const std::array<float, 3>& value) noexcept {
  const auto length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
  if (length < 0.0001F)
    return {0.0F, 1.0F, 0.0F};
  return {value[0] / length, value[1] / length, value[2] / length};
}

void append_gizmo_arrow(std::array<granit::debug_draw_line, 128>& lines, std::size_t& count,
                        const std::array<float, 3>& start, const std::array<float, 3>& vector,
                        float scale, std::uint32_t color) noexcept {
  const auto direction = normalized_or_up(vector);
  const std::array<float, 3> end{start[0] + direction[0] * scale,
                                 start[1] + direction[1] * scale,
                                 start[2] + direction[2] * scale};
  append_gizmo_line(lines, count, start, end, color, 2.5F);
  const std::array<float, 3> side{-direction[1], direction[0], 0.0F};
  const auto side_direction = normalized_or_up(side);
  constexpr float head_length = 0.18F;
  constexpr float head_width = 0.08F;
  const std::array<float, 3> head_base{end[0] - direction[0] * head_length,
                                       end[1] - direction[1] * head_length,
                                       end[2] - direction[2] * head_length};
  const std::array<float, 3> left{head_base[0] + side_direction[0] * head_width,
                                  head_base[1] + side_direction[1] * head_width,
                                  head_base[2] + side_direction[2] * head_width};
  const std::array<float, 3> right{head_base[0] - side_direction[0] * head_width,
                                   head_base[1] - side_direction[1] * head_width,
                                   head_base[2] - side_direction[2] * head_width};
  append_gizmo_line(lines, count, end, left, color, 2.5F);
  append_gizmo_line(lines, count, end, right, color, 2.5F);
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
      result = debug_draw_.initialize(renderer_owner(), {.initial_line_capacity = 128});
    if (result.ok())
      result = create_depth_target();
    if (result.ok())
      result = frame_context_.initialize(renderer());
    if (result.ok())
      result = shader_library_.initialize(renderer(), tutorial_particles::shader_archive());
    if (result.ok())
      result = shader_library_.create_shader("particles.vertex", vertex_shader_);
    if (result.ok())
      result = shader_library_.create_shader("particles.fragment", fragment_shader_);
    if (result.ok())
      result = initialize_buffers();
    if (result.ok())
      result = initialize_pipeline_layout();
    if (result.ok())
      result = create_pipeline();
    if (result.ok())
      reset_particles();
    if (result.ok() &&
        !camera_.focus({.radius = 4.4F}, presentation_info().width, presentation_info().height))
      result = granit::result::invalid_argument;
    return result;
  }

  granit::result on_swapchain_changed(const granit::swapchain_info& info) noexcept override {
    auto result = create_depth_target();
    if (result.ok() && info.format != pipeline_format_)
      result = create_pipeline();
    return result;
  }

  void on_shutdown(granit::result) noexcept override {
    static_cast<void>(debug_draw_.destroy());
    runtime_.shutdown();
    static_cast<void>(pipeline_.reset());
    static_cast<void>(resource_group_.reset());
    static_cast<void>(pipeline_layout_.reset());
    static_cast<void>(resource_layout_.reset());
    static_cast<void>(uniform_buffer_.reset());
    static_cast<void>(particle_buffer_.reset());
    for (auto& mesh : meshes_)
      static_cast<void>(mesh.reset());
    static_cast<void>(index_buffer_.reset());
    static_cast<void>(vertex_buffer_.reset());
    static_cast<void>(depth_view_.reset());
    static_cast<void>(depth_texture_.reset());
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

  void reset_particles() noexcept {
    active_particles_.fill(false);
    active_count_ = 0;
    next_particle_ = 0;
    emission_accumulator_ = 0.0F;
    for (std::uint32_t index = 0; index < maximum_particles; ++index)
      hide_particle(index);
    if (smoke_test_)
      emit_particles(smoke_particles);
  }

  void set_particle_capacity(std::uint32_t capacity) noexcept {
    if (capacity < particle_count_) {
      for (std::uint32_t index = capacity; index < particle_count_; ++index) {
        if (active_particles_[index]) {
          active_particles_[index] = false;
          --active_count_;
        }
        hide_particle(index);
      }
    }
    particle_count_ = capacity;
  }

  void emit_particles(std::uint32_t count) noexcept {
    for (std::uint32_t emitted = 0; emitted < count; ++emitted) {
      std::uint32_t candidate = next_particle_;
      bool found = false;
      for (std::uint32_t offset = 0; offset < maximum_particles; ++offset) {
        candidate = (next_particle_ + offset) % maximum_particles;
        if (!active_particles_[candidate]) {
          found = true;
          break;
        }
      }
      if (!found)
        return;
      spawn_particle(candidate);
      next_particle_ = (candidate + 1) % maximum_particles;
    }
  }

  void spawn_particle(std::uint32_t index) noexcept {
    {
      const auto velocity = initial_velocity(index);
      const float hue = static_cast<float>(index % 9) / 8.0F;
      const auto angular_velocity = initial_angular_velocity(index);
      particles_[index] = {
          .position = {0.0F, -2.2F, 0.0F, 1.0F},
          .velocity = {velocity[0], velocity[1], velocity[2], 0.0F},
          .color = {0.25F + hue * 0.7F, 0.35F + (1.0F - hue) * 0.5F, 1.0F - hue * 0.35F,
                    1.0F},
          .lifetime = lifetime_,
          .age = 0.0F,
          .rotation = {random_component(index, 83U) * 3.14159265F,
                       random_component(index, 89U) * 3.14159265F,
                       random_component(index, 97U) * 3.14159265F},
          .angular_velocity = angular_velocity,
      };
      active_particles_[index] = true;
      ++active_count_;
    }
  }

  void hide_particle(std::uint32_t index) noexcept {
    particle_instances_[index] = {};
    particle_instances_[index].color[3] = 0.0F;
  }

  [[nodiscard]] static float random_component(std::uint32_t index, std::uint32_t salt) noexcept {
    std::uint32_t value = index * 747796405U + salt * 2891336453U + 1013904223U;
    value ^= value >> 16U;
    value *= 2246822519U;
    value ^= value >> 13U;
    return static_cast<float>(value & 0xffffU) / 32767.5F - 1.0F;
  }

  [[nodiscard]] std::array<float, 3> initial_velocity(std::uint32_t index) const noexcept {
    std::array<float, 3> base = direction_;
    const auto length = std::sqrt(base[0] * base[0] + base[1] * base[1] + base[2] * base[2]);
    if (length < 0.0001F)
      base = {0.0F, 1.0F, 0.0F};
    else
      for (auto& component : base)
        component /= length;
    std::array<float, 3> velocity{
        base[0] + random_component(index, 11U) * 0.65F,
        base[1] + random_component(index, 23U) * 0.65F,
        base[2] + random_component(index, 37U) * 0.65F};
    const auto velocity_length = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] +
                                           velocity[2] * velocity[2]);
    for (auto& component : velocity)
      component = component / velocity_length * (0.45F +
        (random_component(index, 53U) + 1.0F) * 0.18F);
    return velocity;
  }

  [[nodiscard]] std::array<float, 3> initial_angular_velocity(
      std::uint32_t index) const noexcept {
    return {random_component(index, 71U) < 0.0F ? -angular_speed_[0] : angular_speed_[0],
            random_component(index, 73U) < 0.0F ? -angular_speed_[1] : angular_speed_[1],
            random_component(index, 79U) < 0.0F ? -angular_speed_[2] : angular_speed_[2]};
  }

  void update_particles(float delta_seconds) noexcept {
    const auto step = smoke_test_ ? 0.75F : std::min(delta_seconds, 0.05F);
    const auto simulation_count = smoke_test_ ? smoke_particles : particle_count_;
    if (!smoke_test_ && emission_mode_ == continuous_emission) {
      emission_accumulator_ += delta_seconds * emission_rate_;
      const auto count = static_cast<std::uint32_t>(emission_accumulator_);
      emission_accumulator_ -= static_cast<float>(count);
      emit_particles(count);
    }
    for (std::uint32_t index = 0; index < simulation_count; ++index) {
      if (!active_particles_[index]) {
        hide_particle(index);
        continue;
      }
      auto& particle = particles_[index];
      particle.age += step;
      if (particle.age >= particle.lifetime) {
        if (smoke_test_) {
          active_particles_[index] = false;
          --active_count_;
          hide_particle(index);
          spawn_particle(index);
          continue;
        }
        active_particles_[index] = false;
        --active_count_;
        hide_particle(index);
        continue;
      }
      if (animate_ || smoke_test_) {
        particle.velocity[0] += direction_[0] * step;
        for (std::size_t axis = 0; axis < 3; ++axis)
          particle.velocity[axis] += (direction_[axis] + acceleration_[axis]) * step;
        for (std::size_t axis = 0; axis < 3; ++axis)
          particle.position[axis] += particle.velocity[axis] * step;
        for (std::size_t axis = 0; axis < 3; ++axis) {
          particle.angular_velocity[axis] = particle.angular_velocity[axis] < 0.0F
                                                ? -angular_speed_[axis]
                                                : angular_speed_[axis];
          particle.rotation[axis] += particle.angular_velocity[axis] * step;
        }
      }
      const float life = std::clamp(1.0F - particle.age / particle.lifetime, 0.0F, 1.0F);
      particle_instances_[index] = {
          .position_scale = {particle.position[0], particle.position[1], particle.position[2],
                             particle_size_},
          .color = {particle.color[0] * particle_color_[0], particle.color[1] * particle_color_[1],
                    particle.color[2] * particle_color_[2],
                    particle.color[3] * opacity_ * (fade_ ? life : 1.0F)},
          .shape_scale = {shape_ == box_shape ? 0.7F : 1.0F,
                          shape_ == box_shape ? 1.35F : 1.0F,
                          shape_ == box_shape ? 0.7F : 1.0F, 0.0F},
          .rotation = {particle.rotation[0], particle.rotation[1], particle.rotation[2], 0.0F},
      };
    }
  }

  void sort_particles(const granit::math::float3& camera_position) noexcept {
    const auto draw_count = smoke_test_ ? smoke_particles : particle_count_;
    std::array<std::uint32_t, maximum_particles> order{};
    std::uint32_t active_count = 0;
    for (std::uint32_t index = 0; index < draw_count; ++index) {
      if (active_particles_[index])
        order[active_count++] = index;
    }
    const auto distance_squared = [&](std::uint32_t index) {
      const auto& position = particles_[index].position;
      const auto x = position[0] - camera_position.x;
      const auto y = position[1] - camera_position.y;
      const auto z = position[2] - camera_position.z;
      return x * x + y * y + z * z;
    };
    std::sort(order.begin(), order.begin() + active_count,
              [&](std::uint32_t left, std::uint32_t right) {
                return distance_squared(left) > distance_squared(right);
              });
    const auto unsorted = particle_instances_;
    for (std::uint32_t output = 0; output < draw_count; ++output) {
      if (output < active_count)
        particle_instances_[output] = unsorted[order[output]];
      else
        hide_particle(output);
    }
  }

  granit::result update_gizmos() noexcept {
    auto result = debug_draw_.clear();
    if (result.failed() || !gizmos_)
      return result;

    std::array<granit::debug_draw_line, 128> lines{};
    std::size_t line_count = 0;
    const auto emitter_color = rgba8(255, 64, 224);
    const auto direction_color = rgba8(64, 224, 255);
    const auto acceleration_color = rgba8(255, 160, 32);
    const auto light_color = rgba8(255, 240, 64);
    append_gizmo_sphere(lines, line_count, emitter_position, 0.14F, emitter_color);
    append_gizmo_axes(lines, line_count, emitter_position, 0.42F);
    append_gizmo_arrow(lines, line_count, emitter_position, direction_, 1.25F, direction_color);
    if (acceleration_[0] * acceleration_[0] + acceleration_[1] * acceleration_[1] +
            acceleration_[2] * acceleration_[2] >
        0.0001F)
      append_gizmo_arrow(lines, line_count, emitter_position, acceleration_, 0.9F,
                         acceleration_color);

    append_gizmo_square(lines, line_count, light_plane_position, 0.42F, light_color);
    append_gizmo_arrow(lines, line_count, light_plane_position, light_travel_direction, 1.0F,
                       light_color);
    return debug_draw_.append_lines(std::span{lines}.first(line_count));
  }

  granit::result initialize_buffers() noexcept {
    auto result = vertex_buffer_.initialize(
        renderer_owner(), {.size = sizeof(tutorial_particles::vertices),
                            .usage = granit::buffer_usage::vertex |
                                     granit::buffer_usage::transfer_destination});
    if (result.ok())
      result = index_buffer_.initialize(
          renderer_owner(), {.size = sizeof(tutorial_particles::indices),
                              .usage = granit::buffer_usage::index |
                                       granit::buffer_usage::transfer_destination});
    if (result.ok())
      result = particle_buffer_.initialize(
          renderer_owner(), {.size = sizeof(particle_instances_),
                              .usage = granit::buffer_usage::storage |
                                       granit::buffer_usage::transfer_destination});
    granit::renderer_limits limits;
    if (result.ok())
      result = renderer_owner().get_limits(limits);
    if (result.ok()) {
      const auto alignment = limits.uniform_buffer_offset_alignment;
      uniform_stride_ = (sizeof(scene_uniforms) + alignment - 1) / alignment * alignment;
      result = uniform_buffer_.initialize(
          renderer_owner(), {.size = uniform_stride_ * GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
                              .usage = granit::buffer_usage::uniform |
                                       granit::buffer_usage::transfer_destination});
    }
    granit::upload_batch upload;
    if (result.ok())
      result = upload.initialize(renderer_owner());
    if (result.ok())
      result = upload.write_buffer(vertex_buffer_.ref(), 0,
                                   std::as_bytes(std::span{tutorial_particles::vertices}));
    if (result.ok())
      result = upload.write_buffer(index_buffer_.ref(), 0,
                                   std::as_bytes(std::span{tutorial_particles::indices}));
    if (result.ok())
      result = upload.write_buffer(particle_buffer_.ref(), 0,
                                   std::as_bytes(std::span{particle_instances_}));
    if (result.ok())
      result = upload.submit();
    if (result.ok())
      result = initialize_meshes();
    return result;
  }

  granit::result initialize_meshes() noexcept {
    const std::array attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{.location = 1, .format = granit::vertex_format::float32x3,
                                 .offset = sizeof(float) * 3}};
    const std::array buffers{granit::mesh_vertex_buffer{
        .buffer = vertex_buffer_.ref(),
        .layout = {.stride = sizeof(tutorial_particles::vertex), .attributes = attributes}}};
    auto result = meshes_[sphere_shape].initialize(
        renderer_owner(), {.vertex_buffers = buffers,
                            .index_buffer = index_buffer_.ref(),
                            .index_format = granit::index_type::uint16,
                            .index_count = 144});
    if (result.ok())
      result = meshes_[cube_shape].initialize(
          renderer_owner(), {.vertex_buffers = buffers,
                              .index_buffer = index_buffer_.ref(),
                              .index_format = granit::index_type::uint16,
                              .index_count = 36,
                              .first_index = 144,
                              .vertex_offset = 26});
    return result;
  }

  granit::result initialize_pipeline_layout() noexcept {
    const std::array entries{
        granit::bind_group_layout_entry{.binding = 0,
                                        .type = granit::binding_type::dynamic_uniform_buffer,
                                        .visibility = granit::shader_stage_flags::vertex},
        granit::bind_group_layout_entry{.binding = 1,
                                        .type = granit::binding_type::read_only_storage_buffer,
                                        .visibility = granit::shader_stage_flags::vertex},
    };
    auto result = resource_layout_.initialize(renderer_owner(), entries);
    const std::array layouts{resource_layout_.ref()};
    if (result.ok())
      result = pipeline_layout_.initialize(renderer_owner(), layouts);
    const std::array resources{
        granit::bind_group_entry{.binding = 0, .resource = uniform_buffer_.ref(),
                                 .size = sizeof(scene_uniforms)},
        granit::bind_group_entry{.binding = 1, .resource = particle_buffer_.ref(),
                                 .size = sizeof(particle_instances_)},
    };
    if (result.ok())
      result = resource_group_.initialize(renderer_owner(), resource_layout_, resources);
    return result;
  }

  granit::result create_depth_target() noexcept {
    auto result = depth_view_.reset();
    if (result.ok())
      result = depth_texture_.reset();
    if (result.ok())
      result = depth_texture_.initialize(
          renderer_owner(), {.format = granit::texture_format::d32_float,
                              .usage = granit::texture_usage::depth_stencil_attachment,
                              .width = presentation_info().width,
                              .height = presentation_info().height});
    if (result.ok())
      result = depth_view_.initialize(renderer_owner(), depth_texture_);
    return result;
  }

  granit::result create_pipeline() noexcept {
    auto result = pipeline_.reset();
    if (result.failed())
      return result;
    const auto format = presentation_info().format;
    const std::array attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{.location = 1, .format = granit::vertex_format::float32x3,
                                 .offset = sizeof(float) * 3}};
    const std::array vertex_buffers{granit::vertex_buffer_layout{
        .stride = sizeof(tutorial_particles::vertex), .attributes = attributes}};
    const std::array blends{granit::color_blend_state{
        .source_color_factor = granit::blend_factor::source_alpha,
        .destination_color_factor = granit::blend_factor::one_minus_source_alpha,
        .source_alpha_factor = granit::blend_factor::one,
        .destination_alpha_factor = granit::blend_factor::one_minus_source_alpha}};
    result = pipeline_.initialize(
        renderer_owner(), {.layout = pipeline_layout_.ref(),
                            .vertex_shader = vertex_shader_.ref(),
                            .fragment_shader = fragment_shader_.ref(),
                            .color_formats = std::span{&format, 1},
                            .depth_stencil_format = granit::texture_format::d32_float,
                            .vertex_buffers = vertex_buffers,
                            .primitive = {.cull = granit::cull_mode::back},
                            .depth = granit::depth_state{.test_enabled = true,
                                                         .write_enabled = false},
                            .color_blends = blends,
                            .depth_bias = std::nullopt});
    if (result.ok())
      pipeline_format_ = format;
    return result;
  }

  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    camera_input_.begin_frame();
    granit::window_state window_state;
    auto result = app_window().get_state(window_state);
    if (result.ok())
      result = runtime_.begin_frame(window_state, frame.delta_seconds,
                                    {.name = "09 Particle Generator",
                                     .description = "Instanced shapes, motion and lifetimes",
                                     .frame = rendered_frames()});
    if (result.ok()) {
      int count = static_cast<int>(particle_count_);
      ImGui::SliderInt("Particles", &count, 1, static_cast<int>(maximum_particles));
      if (static_cast<std::uint32_t>(count) != particle_count_) {
        set_particle_capacity(static_cast<std::uint32_t>(count));
      }
      const char* emission_modes[] = {"Burst", "Continuous"};
      int selected_emission_mode = static_cast<int>(emission_mode_);
      if (ImGui::Combo("Emission mode", &selected_emission_mode, emission_modes, 2)) {
        emission_mode_ = static_cast<std::uint32_t>(selected_emission_mode);
        reset_particles();
      }
      ImGui::SliderFloat("Emission rate", &emission_rate_, 1.0F, 256.0F);
      int burst_count = static_cast<int>(burst_count_);
      ImGui::SliderInt("Burst count", &burst_count, 1, static_cast<int>(maximum_particles));
      burst_count_ = static_cast<std::uint32_t>(burst_count);
      if (ImGui::Button("Emit burst"))
        emit_particles(burst_count_);
      const char* shapes[] = {"Sphere", "Cube", "Box"};
      int selected_shape = static_cast<int>(shape_);
      if (ImGui::Combo("Shape", &selected_shape, shapes, 3))
        shape_ = static_cast<std::uint32_t>(selected_shape);
      ImGui::ColorEdit3("Color", particle_color_.data());
      ImGui::SliderFloat("Opacity", &opacity_, 0.05F, 1.0F);
      ImGui::SliderFloat3("Direction", direction_.data(), -1.0F, 1.0F);
      ImGui::SliderFloat3("Acceleration", acceleration_.data(), -2.0F, 2.0F);
      ImGui::SliderFloat3("Angular speed", angular_speed_.data(), 0.0F, 12.0F);
      ImGui::SliderFloat("Ambient", &ambient_strength_, 0.0F, 0.3F);
      ImGui::Checkbox("Normal debug", &normal_debug_);
      ImGui::Checkbox("Diffuse debug", &diffuse_debug_);
      ImGui::Checkbox("Gizmos", &gizmos_);
      ImGui::SliderFloat("Lifetime", &lifetime_, 0.25F, 8.0F);
      ImGui::SliderFloat("Size", &particle_size_, 0.05F, 0.45F);
      ImGui::Checkbox("Fade out", &fade_);
      ImGui::Checkbox("Animate", &animate_);
      if (ImGui::Button("Reset particles"))
        reset_particles();
      ImGui::Text("Active particles: %u / %u", active_count_, particle_count_);
      ImGui::Text("Draw instances: %u", smoke_test_ ? smoke_particles : particle_count_);
      result = runtime_.end_frame();
    }
    if (result.failed())
      return result;
    const auto input = camera_input_.finish(runtime_.wants_mouse(), runtime_.wants_keyboard());
    if (!smoke_test_ && !camera_.update(input, frame.swapchain.width, frame.swapchain.height))
      return granit::result::invalid_argument;
    granit::example::camera::camera_matrices matrices;
    if (!camera_.matrices(frame.swapchain.width, frame.swapchain.height, matrices))
      return granit::result::invalid_argument;
    update_particles(frame.delta_seconds);
    sort_particles(matrices.position);
    result = update_gizmos();
    if (result.failed())
      return result;
    granit::frame_recording recording;
    result = frame_context_.begin(frame.acquired, recording);
    scene_uniforms uniforms{.view_projection = matrices.view_projection,
                            .light_direction = {light_direction[0], light_direction[1],
                                                light_direction[2], 0.0F},
                            .ambient = {ambient_strength_, ambient_strength_, ambient_strength_,
                                        0.0F},
                            .debug = {normal_debug_ ? 1.0F : 0.0F,
                                      diffuse_debug_ ? 1.0F : 0.0F, 0.0F, 0.0F}};
    const auto uniform_offset = uniform_stride_ * recording.frame_slot();
    if (result.ok())
      result = particle_buffer_.write(0, std::as_bytes(std::span{particle_instances_}));
    if (result.ok())
      result = uniform_buffer_.write(uniform_offset, std::as_bytes(std::span{&uniforms, 1}));
    const granit::viewport viewport{0, 0, static_cast<float>(frame.swapchain.width),
                                    static_cast<float>(frame.swapchain.height), 0, 1};
    const granit::scissor scissor{0, 0, frame.swapchain.width, frame.swapchain.height};
    const granit::color_attachment_desc color{.view = frame.backbuffer.view,
                                              .resolve_view = {},
                                              .clear_value = {.red = 0.025F,
                                                              .green = 0.035F,
                                                              .blue = 0.07F,
                                                              .alpha = 1.0F}};
    const granit::depth_stencil_attachment_desc depth{.view = depth_view_.ref(),
                                                      .clear_value = {.depth = 1.0F}};
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                           .depth_stencil_attachment = &depth,
                                           .area = {0, 0, frame.swapchain.width,
                                                    frame.swapchain.height}};
    auto& recorder = recording.recorder();
    const auto mesh_index = shape_ == sphere_shape ? sphere_shape : cube_shape;
    if (result.ok())
      result = recorder.set_viewports(0, std::span{&viewport, 1});
    if (result.ok())
      result = recorder.set_scissors(0, std::span{&scissor, 1});
    if (result.ok())
      result = recorder.bind_graphics_pipeline(pipeline_);
    if (result.ok())
      result = meshes_[mesh_index].bind(recorder);
    const std::array dynamic_offsets{static_cast<std::uint32_t>(uniform_offset)};
    if (result.ok())
      result = recorder.bind_graphics_group(pipeline_layout_, 0, resource_group_, dynamic_offsets);
    if (result.ok())
      result = recorder.begin_rendering(rendering);
    if (result.ok())
      result = meshes_[mesh_index].draw(
          recorder, {.instance_count = smoke_test_ ? smoke_particles : particle_count_});
    if (result.ok())
      result = recorder.end_rendering();
    if (result.ok() && gizmos_) {
      const bool encode_srgb = frame.swapchain.format == granit::texture_format::rgba8_unorm ||
                               frame.swapchain.format == granit::texture_format::bgra8_unorm;
      result = debug_draw_.record_world(
          recorder,
          {.color = frame.backbuffer.view,
           .color_format = frame.swapchain.format,
           .depth = depth_view_.ref(),
           .depth_format = granit::texture_format::d32_float,
           .width = frame.swapchain.width,
           .height = frame.swapchain.height,
           .view_projection = matrices.view_projection,
           .color_load_operation = granit::attachment_load_operation::load,
           .depth_load_operation = granit::attachment_load_operation::load,
           .encode_srgb = encode_srgb});
    }
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
  granit::debug_draw_list debug_draw_;
  granit::shader_library shader_library_;
  granit::shader vertex_shader_, fragment_shader_;
  granit::buffer vertex_buffer_, index_buffer_, particle_buffer_, uniform_buffer_;
  std::array<granit::mesh, 2> meshes_;
  std::array<particle_state, maximum_particles> particles_{};
  std::array<particle_instance, maximum_particles> particle_instances_{};
  granit::texture depth_texture_;
  granit::texture_view depth_view_;
  granit::bind_group_layout resource_layout_;
  granit::pipeline_layout pipeline_layout_;
  granit::bind_group resource_group_;
  granit::graphics_pipeline pipeline_;
  granit::texture_format pipeline_format_{granit::texture_format::undefined};
  std::uint64_t uniform_stride_{};
  std::uint32_t particle_count_{maximum_particles};
  std::uint32_t active_count_{};
  std::uint32_t next_particle_{};
  std::uint32_t emission_mode_{continuous_emission};
  std::uint32_t burst_count_{32};
  std::uint32_t shape_{sphere_shape};
  std::array<float, 4> particle_color_{1.0F, 1.0F, 1.0F, 1.0F};
  std::array<float, 3> direction_{0.0F, 0.15F, 0.0F};
  std::array<float, 3> acceleration_{};
  std::array<float, 3> angular_speed_{2.5F, 2.5F, 2.5F};
  float emission_rate_{32.0F};
  float emission_accumulator_{};
  float lifetime_{4.0F};
  float particle_size_{0.18F};
  float opacity_{0.4F};
  float ambient_strength_{0.06F};
  bool animate_{true};
  bool fade_{true};
  bool gizmos_{true};
  bool normal_debug_{};
  bool diffuse_debug_{};
  bool smoke_test_{};
  std::array<bool, maximum_particles> active_particles_{};
};

tutorial_application application;
} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_09_rendered_frames() noexcept {
  return application.rendered_frames();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_09_recreate_count() noexcept {
  return application.completed_recreates();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_09_feature_value() noexcept {
  return application.particle_count();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_09_canvas_items() noexcept {
  return application.canvas_items();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_09_ready() noexcept {
  return application.ready() ? 1 : 0;
}
#endif

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";
  application.set_smoke_test(smoke_test);
  const auto executable_path = argument_count > 0 && arguments[0] != nullptr ? arguments[0] : "";
  const auto result = application.run({.executable_path = executable_path,
                                       .title = "Granit Particle Generator",
                                       .renderer = {.application_name = "Granit Particle Generator",
                                                    .presentation = granit::presentation_mode::enabled},
                                       .swapchain = {},
                                       .smoke_test = smoke_test});
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  return result.failed() ? report_failure("application run", result) : 0;
}
