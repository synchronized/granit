// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>
#include <granit/pipeline/material.hpp>
#include <granit/pipeline/mesh.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/window.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "model_data.hpp"

#ifndef GRANIT_TUTORIAL_06_SHADER_LIBRARY
#error "GRANIT_TUTORIAL_06_SHADER_LIBRARY must point to the generated Shader Library"
#endif
#ifndef GRANIT_TUTORIAL_06_MATERIAL
#error "GRANIT_TUTORIAL_06_MATERIAL must point to the generated Material archive"
#endif

namespace {

using granit::math::matrix4;

matrix4 multiply(const matrix4& left, const matrix4& right) {
  matrix4 output{};
  for (std::size_t column = 0; column < 4; ++column) {
    for (std::size_t row = 0; row < 4; ++row) {
      for (std::size_t inner = 0; inner < 4; ++inner)
        output[column * 4 + row] += left[inner * 4 + row] * right[column * 4 + inner];
    }
  }
  return output;
}

matrix4 rotation_y(float angle) {
  const float cosine = std::cos(angle);
  const float sine = std::sin(angle);
  return {{cosine, 0, -sine, 0, 0, 1, 0, 0, sine, 0, cosine, 0, 0, 0, 0, 1}};
}

matrix4 view_matrix() { return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -4, 1}}; }

matrix4 projection_matrix(float aspect) {
  constexpr float near_plane = 0.1F;
  constexpr float far_plane = 100.0F;
  constexpr float vertical_scale = 1.7320508F;
  return {{vertical_scale / aspect, 0, 0, 0, 0, -vertical_scale, 0, 0, 0, 0,
           far_plane / (near_plane - far_plane), -1, 0, 0,
           (near_plane * far_plane) / (near_plane - far_plane), 0}};
}

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

std::vector<std::byte> read_file(const char* path) {
  std::ifstream stream{path, std::ios::binary | std::ios::ate};
  if (!stream)
    return {};
  const auto size = stream.tellg();
  if (size <= 0)
    return {};
  stream.seekg(0, std::ios::beg);
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  stream.read(reinterpret_cast<char*>(bytes.data()), size);
  return stream ? bytes : std::vector<std::byte>{};
}

granit::result poll_window_events(granit::window_system& window_system, bool& running,
                                  bool& recreate) {
  granit::window_event event;
  granit::result result;
  while ((result = window_system.poll(event)).ok()) {
    if (event.type == granit::window_event_type::close_requested)
      running = false;
    if (event.type == granit::window_event_type::resized ||
        event.type == granit::window_event_type::scale_changed ||
        event.type == granit::window_event_type::native_handle_changed) {
      recreate = true;
    }
  }
  return result == granit::result::not_ready ? granit::result::success : result;
}

granit::result poll_input_events(granit::window_system& window_system, bool& running) {
  granit::input_event event;
  granit::result result;
  while ((result = window_system.poll(event)).ok()) {
    if (event.type == granit::input_event_type::key &&
        event.data.key.action == granit::key_action::released &&
        event.data.key.physical == granit::physical_key::escape) {
      running = false;
    }
  }
  return result == granit::result::not_ready ? granit::result::success : result;
}

enum class application_phase { renderer_initializing, running, stopped };

class tutorial_application {
public:
  granit::result initialize(bool smoke_test) noexcept {
    smoke_test_ = smoke_test;
    shader_archive_ = read_file(GRANIT_TUTORIAL_06_SHADER_LIBRARY);
    material_archive_ = read_file(GRANIT_TUTORIAL_06_MATERIAL);
    if (shader_archive_.empty() || material_archive_.empty()) {
      std::cerr << "Failed to read generated Shader Library or Material archive\n";
      return granit::result::invalid_argument;
    }

    auto result = window_system_.initialize();
    if (result.failed())
      return result;
    result = window_.initialize(window_system_,
                                {.title = "Granit Tutorial 06", .width = 1280, .height = 720});
    if (result.failed())
      return result;
    return renderer_.initialize({.application_name = "Granit Tutorial 06",
                                 .presentation = granit::presentation_mode::enabled});
  }

  granit::result tick(granit::window_loop_action& action) noexcept {
    auto result = poll_window_events(window_system_, running_, recreate_);
    if (result.ok())
      result = poll_input_events(window_system_, running_);
    if (result.failed())
      return result;
    if (!running_) {
      action = granit::window_loop_action::stop;
      return smoke_complete() || !smoke_test_ ? granit::result::success
                                              : granit::result::initialization_failed;
    }

    result = renderer_.process_events();
    if (result.failed())
      return result;
    if (phase_ == application_phase::renderer_initializing) {
      granit::renderer_status status;
      result = renderer_.get_status(status);
      if (result.failed())
        return result;
      if (status.state == granit::renderer_state::initializing) {
        action = granit::window_loop_action::idle;
        return granit::result::success;
      }
      if (status.state != granit::renderer_state::ready)
        return status.failure_result;
      result = initialize_presentation();
      if (result == granit::result::not_ready) {
        action = granit::window_loop_action::idle;
        return granit::result::success;
      }
      if (result.failed())
        return result;
      phase_ = application_phase::running;
    }

    result = window_.get_state(window_state_);
    if (result.failed())
      return result;
    const auto width = window_state_.framebuffer_width;
    const auto height = window_state_.framebuffer_height;
    if (width == 0 || height == 0) {
      action = granit::window_loop_action::idle;
      return granit::result::success;
    }
    if (recreate_ || width != swapchain_info_.width || height != swapchain_info_.height) {
      result = swapchain_.recreate({.width = width, .height = height});
      if (result == granit::result::not_ready) {
        action = granit::window_loop_action::idle;
        return granit::result::success;
      }
      if (result.failed())
        return result;
      result = swapchain_.query_info(swapchain_info_);
      if (result.failed())
        return result;
      recreate_ = false;
      ++completed_recreates_;
    }

    result = render_frame();
    if (result == granit::result::out_of_date) {
      recreate_ = true;
      return granit::result::success;
    }
    if (result.failed())
      return result;

    ++rendered_frames_;
    if (smoke_test_ && rendered_frames_ == 1)
      recreate_ = true;
    if (smoke_complete())
      action = granit::window_loop_action::stop;
    return granit::result::success;
  }

  void shutdown(granit::result) noexcept {
    phase_ = application_phase::stopped;
    static_cast<void>(scene_.reset());
    static_cast<void>(pipeline_.reset());
    static_cast<void>(material_.reset());
    static_cast<void>(mesh_.reset());
    static_cast<void>(index_buffer_.reset());
    static_cast<void>(vertex_buffer_.reset());
    static_cast<void>(material_sampler_.reset());
    for (auto& view : material_views_)
      static_cast<void>(view.reset());
    for (auto& texture : material_textures_)
      static_cast<void>(texture.reset());
    static_cast<void>(shader_library_.reset());
    static_cast<void>(swapchain_.reset());
    static_cast<void>(surface_.reset());
    static_cast<void>(renderer_.reset());
    static_cast<void>(window_.reset());
    static_cast<void>(window_system_.reset());
  }

  [[nodiscard]] granit::window_system& system() noexcept { return window_system_; }

private:
  granit::result initialize_presentation() noexcept {
    auto result = window_.get_state(window_state_);
    if (result.failed())
      return result;
    if (window_state_.framebuffer_width == 0 || window_state_.framebuffer_height == 0)
      return granit::result::not_ready;
    result = window_.create_surface(renderer_, surface_);
    if (result.ok()) {
      result = swapchain_.initialize(
          renderer_, surface_,
          {.width = window_state_.framebuffer_width, .height = window_state_.framebuffer_height});
    }
    if (result.ok())
      result = swapchain_.query_info(swapchain_info_);
    if (result.ok())
      result = shader_library_.initialize(renderer_, shader_archive_);
    if (result.ok())
      result = initialize_geometry();
    if (result.ok())
      result = initialize_material();
    if (result.ok())
      result = pipeline_.initialize(renderer_);
    return result;
  }

  granit::result initialize_geometry() noexcept {
    auto result = vertex_buffer_.initialize(
        renderer_,
        {.size = sizeof(tutorial_model::vertices), .usage = granit::buffer_usage::vertex},
        std::as_bytes(std::span{tutorial_model::vertices}));
    if (result.ok()) {
      result = index_buffer_.initialize(
          renderer_,
          {.size = sizeof(tutorial_model::indices), .usage = granit::buffer_usage::index},
          std::as_bytes(std::span{tutorial_model::indices}));
    }

    const std::array attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{
            .location = 1, .format = granit::vertex_format::float32x3, .offset = sizeof(float) * 3},
        granit::vertex_attribute{
            .location = 2, .format = granit::vertex_format::float32x4, .offset = sizeof(float) * 6},
        granit::vertex_attribute{.location = 3,
                                 .format = granit::vertex_format::float32x2,
                                 .offset = sizeof(float) * 10},
    };
    const granit::mesh_vertex_buffer binding{
        .buffer = vertex_buffer_.ref(),
        .layout = {.stride = sizeof(tutorial_model::vertex), .attributes = attributes},
    };
    if (result.ok()) {
      result = mesh_.initialize(
          renderer_, {.topology = granit::primitive_topology::triangle_list,
                      .vertex_buffers = std::span{&binding, 1},
                      .index_buffer = index_buffer_.ref(),
                      .index_format = granit::index_type::uint16,
                      .index_count = static_cast<std::uint32_t>(tutorial_model::indices.size())});
    }
    return result;
  }

  granit::result initialize_material() noexcept {
    constexpr std::array<std::array<std::uint8_t, 4>, 5> pixels{{
        {255, 255, 255, 255},
        {0, 128, 64, 255},
        {128, 128, 255, 255},
        {255, 255, 255, 255},
        {0, 0, 0, 255},
    }};
    auto result = granit::result::success;
    for (std::size_t index = 0; index < material_textures_.size() && result.ok(); ++index) {
      result = material_textures_[index].initialize(
          renderer_,
          {.format = granit::texture_format::rgba8_unorm,
           .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
           .width = 1,
           .height = 1});
      if (result.ok())
        result = material_textures_[index].write(std::as_bytes(std::span{pixels[index]}), {}, {});
      if (result.ok())
        result = material_views_[index].initialize(renderer_, material_textures_[index]);
    }
    if (result.ok())
      result = material_sampler_.initialize(renderer_, {});

    const granit::math::float4 base_color{0.82F, 0.24F, 0.08F, 1.0F};
    const float metallic = 0.25F;
    const float roughness = 0.38F;
    const std::array updates{
        granit::material_parameter_update::value(granit::material_parameter_id("base_color"),
                                                 granit::material_parameter_type::float4,
                                                 std::as_bytes(std::span{&base_color, 1})),
        granit::material_parameter_update::value(granit::material_parameter_id("metallic"),
                                                 granit::material_parameter_type::float32,
                                                 std::as_bytes(std::span{&metallic, 1})),
        granit::material_parameter_update::value(
            granit::material_parameter_id("perceptual_roughness"),
            granit::material_parameter_type::float32, std::as_bytes(std::span{&roughness, 1})),
        granit::material_parameter_update::texture_binding(
            granit::material_parameter_id("base_color_texture"), material_views_[0].ref()),
        granit::material_parameter_update::texture_binding(
            granit::material_parameter_id("metallic_roughness_texture"), material_views_[1].ref()),
        granit::material_parameter_update::texture_binding(
            granit::material_parameter_id("normal_texture"), material_views_[2].ref()),
        granit::material_parameter_update::texture_binding(
            granit::material_parameter_id("occlusion_texture"), material_views_[3].ref()),
        granit::material_parameter_update::texture_binding(
            granit::material_parameter_id("emissive_texture"), material_views_[4].ref()),
        granit::material_parameter_update::sampler_binding(
            granit::material_parameter_id("pbr_sampler"), material_sampler_.ref()),
    };
    if (result.ok()) {
      result = material_.initialize(renderer_, {.archive = material_archive_,
                                                .initial_updates = updates,
                                                .shader_library = shader_library_.ref()});
    }
    return result;
  }

  granit::result update_scene() noexcept {
    auto result = scene_.reset();
    if (result.failed())
      return result;

    const auto aspect =
        static_cast<float>(swapchain_info_.width) / static_cast<float>(swapchain_info_.height);
    const auto model = rotation_y(static_cast<float>(rendered_frames_) * 0.02F);
    const auto view = view_matrix();
    const auto projection = projection_matrix(aspect);
    const granit::scene_view scene_view{
        .view = view,
        .projection = projection,
        .view_projection = multiply(projection, view),
        .camera_position = {0, 0, 4},
        .viewport_x = 0,
        .viewport_y = 0,
        .viewport_width = static_cast<float>(swapchain_info_.width),
        .viewport_height = static_cast<float>(swapchain_info_.height),
        .layer_mask = UINT64_MAX,
    };
    const granit::scene_renderable renderable{
        .model = model,
        .normal_matrix = model,
        .bounds_center = {0, 0, 0},
        .bounds_radius = 1.7320508F,
        .layer_mask = UINT64_MAX,
        .sort_key = 0,
        .payload = 1,
        .object_id = 1,
        .reserved = 0,
    };
    const granit::scene_directional_light light{
        .direction_to_light = {0.365148F, 0.912871F, 0.182574F},
        .radiance = {4.0F, 3.8F, 3.4F},
        .layer_mask = UINT64_MAX,
    };
    return scene_.initialize(renderer_, {.views = std::span{&scene_view, 1},
                                         .renderables = std::span{&renderable, 1},
                                         .directional_lights = std::span{&light, 1}});
  }

  granit::result render_frame() noexcept {
    auto result = update_scene();
    if (result.failed())
      return result;

    granit::acquired_frame frame;
    result = swapchain_.acquire(frame);
    if (result.failed())
      return result;
    recreate_ = recreate_ || frame.needs_recreate();

    granit::swapchain_backbuffer backbuffer;
    result = swapchain_.backbuffer(frame, backbuffer);
    const granit::render_pipeline_draw_binding binding{
        .payload = 1, .mesh = mesh_.ref(), .material = material_.ref()};
    if (result.ok()) {
      granit::render_pipeline_render_desc render_desc{};
      render_desc.scene = scene_.ref();
      render_desc.output = backbuffer.view;
      render_desc.output_format = swapchain_info_.format;
      render_desc.width = swapchain_info_.width;
      render_desc.height = swapchain_info_.height;
      render_desc.draw_bindings = std::span{&binding, 1};
      render_desc.frame = &frame;
      render_desc.clear_color = {0.025F, 0.03F, 0.045F, 1.0F};
      result = pipeline_.render(render_desc);
    }
    if (result.ok())
      result = swapchain_.present(frame);

    recreate_ = recreate_ || frame.needs_recreate();
    if (result.failed() && frame.valid())
      static_cast<void>(swapchain_.cancel(frame));
    return result;
  }

  [[nodiscard]] bool smoke_complete() const noexcept {
    return smoke_test_ && rendered_frames_ >= 3 && completed_recreates_ >= 1;
  }

  granit::window_system window_system_;
  granit::window window_;
  granit::renderer renderer_;
  granit::surface surface_;
  granit::swapchain swapchain_;
  std::vector<std::byte> shader_archive_;
  std::vector<std::byte> material_archive_;
  granit::shader_library shader_library_;
  std::array<granit::texture, 5> material_textures_;
  std::array<granit::texture_view, 5> material_views_;
  granit::sampler material_sampler_;
  granit::buffer vertex_buffer_;
  granit::buffer index_buffer_;
  granit::mesh mesh_;
  granit::material_instance material_;
  granit::render_pipeline pipeline_;
  granit::scene_snapshot scene_;
  granit::window_state window_state_{};
  granit::swapchain_info swapchain_info_{};
  application_phase phase_{application_phase::renderer_initializing};
  bool running_{true};
  bool recreate_{};
  bool smoke_test_{};
  std::uint32_t rendered_frames_{};
  std::uint32_t completed_recreates_{};
};

tutorial_application application;

} // namespace

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";

  auto result = application.initialize(smoke_test);
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  if (result.failed())
    return report_failure("application initialize", result);
  result = granit::run_window_loop(application.system(), application);
  return result.failed() ? report_failure("application loop", result) : 0;
}
