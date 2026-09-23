// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf/loader.h"
#include "model_viewer/gpu_scene.h"

#include <granit/granit.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/window.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#ifndef GRANIT_SAMPLE_MODEL
#error "GRANIT_SAMPLE_MODEL must point to the tutorial glTF asset"
#endif

namespace {

namespace gltf = granit::example::gltf;
namespace model_viewer = granit::example::model_viewer;
using granit::math::matrix4;

bool read_file(const std::filesystem::path& path, std::vector<std::byte>& output) {
  std::ifstream stream{path, std::ios::binary | std::ios::ate};
  if (!stream)
    return false;
  const auto end = stream.tellg();
  if (end <= 0)
    return false;
  stream.seekg(0, std::ios::beg);
  std::vector<std::byte> bytes(static_cast<std::size_t>(end));
  stream.read(reinterpret_cast<char*>(bytes.data()), end);
  if (!stream)
    return false;
  output = std::move(bytes);
  return true;
}

class file_resolver final : public gltf::resource_resolver {
public:
  explicit file_resolver(std::filesystem::path base) : base_(std::move(base)) {}

  bool resolve(std::string_view path, std::vector<std::byte>& bytes) const override {
    return read_file(base_ / std::filesystem::path{path}, bytes);
  }

private:
  std::filesystem::path base_;
};

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

matrix4 view_matrix(const granit::math::float3& eye) {
  return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -eye.x, -eye.y, -eye.z, 1}};
}

matrix4 projection_matrix(float aspect, float near_plane, float far_plane) {
  constexpr float vertical_scale = 1.7320508F;
  return {{vertical_scale / aspect, 0, 0, 0, 0, -vertical_scale, 0, 0, 0, 0,
           far_plane / (near_plane - far_plane), -1, 0, 0,
           (near_plane * far_plane) / (near_plane - far_plane), 0}};
}

struct camera_frame {
  granit::math::float3 center{};
  float radius{1.0F};
};

camera_frame frame_scene(const model_viewer::gpu_scene_plan& plan) {
  granit::math::float3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                               std::numeric_limits<float>::max()};
  granit::math::float3 maximum{-std::numeric_limits<float>::max(),
                               -std::numeric_limits<float>::max(),
                               -std::numeric_limits<float>::max()};
  for (const auto& draw : plan.draws) {
    minimum.x = std::min(minimum.x, draw.bounds_center.x - draw.bounds_radius);
    minimum.y = std::min(minimum.y, draw.bounds_center.y - draw.bounds_radius);
    minimum.z = std::min(minimum.z, draw.bounds_center.z - draw.bounds_radius);
    maximum.x = std::max(maximum.x, draw.bounds_center.x + draw.bounds_radius);
    maximum.y = std::max(maximum.y, draw.bounds_center.y + draw.bounds_radius);
    maximum.z = std::max(maximum.z, draw.bounds_center.z + draw.bounds_radius);
  }
  if (plan.draws.empty())
    return {};
  const granit::math::float3 center{(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F,
                                    (minimum.z + maximum.z) * 0.5F};
  const auto x = maximum.x - center.x;
  const auto y = maximum.y - center.y;
  const auto z = maximum.z - center.z;
  return {.center = center, .radius = std::max(std::sqrt(x * x + y * y + z * z), 0.1F)};
}

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

granit::result poll_events(granit::window_system& system, bool& running, bool& recreate) {
  granit::window_event window_event;
  granit::result result;
  while ((result = system.poll(window_event)).ok()) {
    if (window_event.type == granit::window_event_type::close_requested)
      running = false;
    if (window_event.type == granit::window_event_type::resized ||
        window_event.type == granit::window_event_type::scale_changed ||
        window_event.type == granit::window_event_type::native_handle_changed) {
      recreate = true;
    }
  }
  if (result != granit::result::not_ready)
    return result;

  granit::input_event input_event;
  while ((result = system.poll(input_event)).ok()) {
    if (input_event.type == granit::input_event_type::key &&
        input_event.data.key.action == granit::key_action::released &&
        input_event.data.key.physical == granit::physical_key::escape) {
      running = false;
    }
  }
  return result == granit::result::not_ready ? granit::result::success : result;
}

enum class application_phase { renderer_initializing, running, stopped };

class tutorial_application {
public:
  granit::result initialize(const std::filesystem::path& asset_path, bool smoke_test) {
    smoke_test_ = smoke_test;
    std::vector<std::byte> document;
    if (!read_file(asset_path, document)) {
      std::cerr << "Failed to read model: " << asset_path << '\n';
      return granit::result::invalid_argument;
    }
    const file_resolver resolver{asset_path.parent_path()};
    const auto load = gltf::load(document, &resolver, cpu_scene_);
    if (!load) {
      std::cerr << "Failed to load model: " << load.diagnostic << '\n';
      return granit::result::invalid_argument;
    }

    auto result = window_system_.initialize();
    if (result.ok()) {
      result = window_.initialize(
          window_system_,
          {.title = "Granit Tutorial 09 - Model Loading", .width = 1280, .height = 720});
    }
    if (result.ok()) {
      result = renderer_.initialize({.application_name = "Granit Tutorial 09",
                                     .presentation = granit::presentation_mode::enabled});
    }
    return result;
  }

  granit::result tick(granit::window_loop_action& action) noexcept {
    auto result = poll_events(window_system_, running_, recreate_);
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
      if ((result = swapchain_.query_info(swapchain_info_)).failed())
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
    if (smoke_test_ && rendered_frames_ == 2)
      recreate_ = true;
    if (smoke_complete())
      action = granit::window_loop_action::stop;
    return granit::result::success;
  }

  void shutdown(granit::result) noexcept {
    phase_ = application_phase::stopped;
    static_cast<void>(snapshot_.reset());
    static_cast<void>(pipeline_.reset());
    gpu_scene_.reset();
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
      result = gpu_scene_.initialize(renderer_, cpu_scene_);
    if (result.ok())
      camera_ = frame_scene(gpu_scene_.plan());
    if (result.ok())
      result = pipeline_.initialize(renderer_, {.enable_fxaa = true, .enable_specular_aa = true});
    return result;
  }

  granit::result update_snapshot() noexcept {
    auto result = snapshot_.reset();
    if (result.failed())
      return result;
    const auto aspect =
        static_cast<float>(swapchain_info_.width) / static_cast<float>(swapchain_info_.height);
    const auto distance = std::max(camera_.radius * 2.8F, 2.0F);
    const granit::math::float3 eye{camera_.center.x, camera_.center.y, camera_.center.z + distance};
    const auto view = view_matrix(eye);
    const auto near_plane = std::max(camera_.radius * 0.01F, 0.01F);
    const auto far_plane = distance + camera_.radius * 4.0F;
    const auto projection = projection_matrix(aspect, near_plane, far_plane);
    const granit::scene_view scene_view{
        .view = view,
        .projection = projection,
        .view_projection = multiply(projection, view),
        .camera_position = eye,
        .viewport_x = 0,
        .viewport_y = 0,
        .viewport_width = static_cast<float>(swapchain_info_.width),
        .viewport_height = static_cast<float>(swapchain_info_.height),
        .layer_mask = UINT64_MAX,
    };
    const granit::scene_directional_light light{
        .direction_to_light = {0.365148F, 0.912871F, 0.182574F},
        .radiance = {4.0F, 3.8F, 3.4F},
        .layer_mask = UINT64_MAX,
    };
    return gpu_scene_.create_snapshot(std::span{&scene_view, 1}, std::span{&light, 1},
                                      std::span<const granit_scene_point_light>{},
                                      std::span<const granit_scene_spot_light>{}, snapshot_);
  }

  granit::result render_frame() noexcept {
    auto result = update_snapshot();
    if (result.failed())
      return result;
    granit::acquired_frame frame;
    result = swapchain_.acquire(frame);
    if (result.failed())
      return result;
    recreate_ = recreate_ || frame.needs_recreate();

    granit::swapchain_backbuffer backbuffer;
    result = swapchain_.backbuffer(frame, backbuffer);
    if (result.ok()) {
      granit::render_pipeline_render_desc desc{};
      desc.scene = snapshot_.ref();
      desc.output = backbuffer.view;
      desc.output_format = swapchain_info_.format;
      desc.width = swapchain_info_.width;
      desc.height = swapchain_info_.height;
      desc.draw_bindings = gpu_scene_.draw_bindings();
      desc.frame = &frame;
      desc.clear_color = {0.025F, 0.03F, 0.045F, 1.0F};
      result = pipeline_.render(desc);
    }
    if (result.ok())
      result = swapchain_.present(frame);
    recreate_ = recreate_ || frame.needs_recreate();
    if (result.failed() && frame.valid())
      static_cast<void>(swapchain_.cancel(frame));
    return result;
  }

  [[nodiscard]] bool smoke_complete() const noexcept {
    return smoke_test_ && rendered_frames_ >= 4 && completed_recreates_ >= 1;
  }

  granit::window_system window_system_;
  granit::window window_;
  granit::renderer renderer_;
  granit::surface surface_;
  granit::swapchain swapchain_;
  granit::render_pipeline pipeline_;
  granit::scene_snapshot snapshot_;
  gltf::scene cpu_scene_;
  model_viewer::gpu_scene gpu_scene_;
  camera_frame camera_;
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
  bool smoke_test = false;
  std::filesystem::path asset_path{GRANIT_SAMPLE_MODEL};
  for (int index = 1; index < argument_count; ++index) {
    const std::string_view argument{arguments[index]};
    if (argument == "--smoke-test")
      smoke_test = true;
    else
      asset_path = arguments[index];
  }

  auto result = application.initialize(asset_path, smoke_test);
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  if (result.failed())
    return report_failure("application initialize", result);
  result = granit::run_window_loop(application.system(), application);
  return result.failed() ? report_failure("application loop", result) : 0;
}
