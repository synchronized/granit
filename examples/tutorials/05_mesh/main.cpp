// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>
#include <granit/pipeline/mesh.hpp>
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

#ifndef GRANIT_TUTORIAL_05_SHADER_LIBRARY
#error "GRANIT_TUTORIAL_05_SHADER_LIBRARY must point to the generated Shader Library"
#endif

namespace {

using matrix4 = std::array<float, 16>;

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

matrix4 make_model_view_projection(float angle, float aspect) {
  const float cosine = std::cos(angle);
  const float sine = std::sin(angle);
  const matrix4 model{cosine, 0, -sine, 0, 0, 1, 0, 0, sine, 0, cosine, 0, 0, 0, 0, 1};
  const matrix4 view{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -4, 1};
  constexpr float near_plane = 0.1F;
  constexpr float far_plane = 100.0F;
  constexpr float vertical_scale = 1.7320508F;
  const matrix4 projection{vertical_scale / aspect,
                           0,
                           0,
                           0,
                           0,
                           -vertical_scale,
                           0,
                           0,
                           0,
                           0,
                           far_plane / (near_plane - far_plane),
                           -1,
                           0,
                           0,
                           (near_plane * far_plane) / (near_plane - far_plane),
                           0};
  return multiply(projection, multiply(view, model));
}

std::uint64_t align_up(std::uint64_t value, std::uint64_t alignment) {
  return alignment == 0 ? value : (value + alignment - 1) / alignment * alignment;
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
    shader_archive_ = read_file(GRANIT_TUTORIAL_05_SHADER_LIBRARY);
    if (shader_archive_.empty()) {
      std::cerr << "Failed to read Shader Library: " << GRANIT_TUTORIAL_05_SHADER_LIBRARY << '\n';
      return granit::result::invalid_argument;
    }

    auto result = window_system_.initialize();
    if (result.failed())
      return result;
    result = window_.initialize(window_system_,
                                {.title = "Granit Tutorial 05", .width = 1280, .height = 720});
    if (result.failed())
      return result;
    return renderer_.initialize({.application_name = "Granit Tutorial 05",
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
      const auto previous_format = swapchain_info_.format;
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
      result = create_depth_target();
      if (result.failed())
        return result;
      if (swapchain_info_.format != previous_format) {
        result = create_pipeline();
        if (result.failed())
          return result;
      }
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
    static_cast<void>(pipeline_.reset());
    static_cast<void>(resource_group_.reset());
    static_cast<void>(layout_.reset());
    static_cast<void>(resource_layout_.reset());
    static_cast<void>(uniform_buffer_.reset());
    static_cast<void>(mesh_.reset());
    static_cast<void>(index_buffer_.reset());
    static_cast<void>(vertex_buffer_.reset());
    static_cast<void>(checker_sampler_.reset());
    static_cast<void>(checker_view_.reset());
    static_cast<void>(checker_texture_.reset());
    static_cast<void>(depth_view_.reset());
    static_cast<void>(depth_texture_.reset());
    static_cast<void>(fragment_shader_.reset());
    static_cast<void>(vertex_shader_.reset());
    static_cast<void>(shader_library_.reset());
    static_cast<void>(frame_context_.reset());
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
      result = create_depth_target();
    if (result.ok())
      result = frame_context_.initialize(renderer_);
    if (result.ok())
      result = shader_library_.initialize(renderer_, shader_archive_);
    if (result.ok())
      result = shader_library_.create_shader("mesh.vertex", vertex_shader_);
    if (result.ok())
      result = shader_library_.create_shader("mesh.fragment", fragment_shader_);
    if (result.ok())
      result = initialize_texture_resources();
    if (result.ok())
      result = initialize_pipeline_layout();
    if (result.ok())
      result = create_pipeline();
    return result;
  }

  granit::result initialize_texture_resources() noexcept {
    auto result = checker_texture_.initialize(
        renderer_,
        {.format = granit::texture_format::rgba8_unorm,
         .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
         .width = 2,
         .height = 2});
    if (result.ok())
      result = checker_view_.initialize(renderer_, checker_texture_);
    if (result.ok()) {
      result =
          checker_sampler_.initialize(renderer_, {.mag_filter = granit::filter::nearest,
                                                  .min_filter = granit::filter::nearest,
                                                  .mip_filter = granit::mipmap_filter::nearest});
    }
    if (result.ok()) {
      result = vertex_buffer_.initialize(
          renderer_,
          {.size = sizeof(tutorial_model::vertices),
           .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination});
    }
    if (result.ok()) {
      result = index_buffer_.initialize(
          renderer_,
          {.size = sizeof(tutorial_model::indices),
           .usage = granit::buffer_usage::index | granit::buffer_usage::transfer_destination});
    }
    granit::renderer_limits limits;
    if (result.ok())
      result = renderer_.get_limits(limits);
    if (result.ok()) {
      uniform_stride_ = align_up(sizeof(matrix4), limits.uniform_buffer_offset_alignment);
      result = uniform_buffer_.initialize(
          renderer_,
          {.size = uniform_stride_ * GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
           .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination});
    }

    constexpr std::array<std::uint8_t, 16> pixels{
        255, 80, 80, 255, 80, 220, 120, 255, 80, 140, 255, 255, 245, 220, 80, 255,
    };
    granit::upload_batch upload;
    if (result.ok())
      result = upload.initialize(renderer_);
    if (result.ok()) {
      result = upload.write_texture(checker_texture_.ref(), std::as_bytes(std::span{pixels}),
                                    {.bytes_per_row = 8, .rows_per_image = 2},
                                    {.width = 2, .height = 2});
    }
    if (result.ok())
      result = upload.write_buffer(vertex_buffer_.ref(), 0,
                                   std::as_bytes(std::span{tutorial_model::vertices}));
    if (result.ok())
      result = upload.write_buffer(index_buffer_.ref(), 0,
                                   std::as_bytes(std::span{tutorial_model::indices}));
    if (result.ok())
      result = upload.submit();
    if (result.ok())
      result = initialize_mesh();
    return result;
  }

  granit::result initialize_mesh() noexcept {
    const std::array attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{
            .location = 1, .format = granit::vertex_format::float32x2, .offset = sizeof(float) * 3},
    };
    const granit::mesh_vertex_buffer binding{
        .buffer = vertex_buffer_.ref(),
        .offset = 0,
        .layout = {.stride = sizeof(tutorial_model::vertex), .attributes = attributes},
    };
    return mesh_.initialize(
        renderer_, {.topology = granit::primitive_topology::triangle_list,
                    .vertex_buffers = std::span{&binding, 1},
                    .index_buffer = index_buffer_.ref(),
                    .index_buffer_offset = 0,
                    .index_format = granit::index_type::uint16,
                    .vertex_count = 0,
                    .index_count = static_cast<std::uint32_t>(tutorial_model::indices.size()),
                    .instance_count = 1,
                    .first_vertex = 0,
                    .first_index = 0,
                    .vertex_offset = 0,
                    .first_instance = 0});
  }

  granit::result initialize_pipeline_layout() noexcept {
    const std::array entries{
        granit::bind_group_layout_entry{.binding = 0,
                                        .type = granit::binding_type::dynamic_uniform_buffer,
                                        .visibility = granit::shader_stage_flags::vertex},
        granit::bind_group_layout_entry{.binding = 1,
                                        .type = granit::binding_type::sampled_texture,
                                        .visibility = granit::shader_stage_flags::fragment},
        granit::bind_group_layout_entry{.binding = 2,
                                        .type = granit::binding_type::sampler,
                                        .visibility = granit::shader_stage_flags::fragment},
    };
    auto result = resource_layout_.initialize(renderer_, entries);
    const std::array layout_refs{resource_layout_.ref()};
    if (result.ok())
      result = layout_.initialize(renderer_, layout_refs);

    const std::array resources{
        granit::bind_group_entry{
            .binding = 0, .resource = uniform_buffer_.ref(), .size = sizeof(matrix4)},
        granit::bind_group_entry{.binding = 1, .resource = checker_view_.ref()},
        granit::bind_group_entry{.binding = 2, .resource = checker_sampler_.ref()},
    };
    if (result.ok())
      result = resource_group_.initialize(renderer_, resource_layout_, resources);
    return result;
  }

  granit::result create_depth_target() noexcept {
    auto result = depth_view_.reset();
    if (result.ok())
      result = depth_texture_.reset();
    if (result.ok()) {
      result = depth_texture_.initialize(renderer_,
                                         {.format = granit::texture_format::d32_float,
                                          .usage = granit::texture_usage::depth_stencil_attachment,
                                          .width = swapchain_info_.width,
                                          .height = swapchain_info_.height});
    }
    if (result.ok())
      result = depth_view_.initialize(renderer_, depth_texture_);
    return result;
  }

  granit::result create_pipeline() noexcept {
    auto result = pipeline_.reset();
    if (result.failed())
      return result;
    const auto format = swapchain_info_.format;
    const std::array attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{
            .location = 1, .format = granit::vertex_format::float32x2, .offset = sizeof(float) * 3},
    };
    const std::array vertex_layouts{granit::vertex_buffer_layout{
        .stride = sizeof(tutorial_model::vertex), .attributes = attributes}};
    return pipeline_.initialize(
        renderer_, {.layout = layout_.ref(),
                    .vertex_shader = vertex_shader_.ref(),
                    .fragment_shader = fragment_shader_.ref(),
                    .color_formats = std::span{&format, 1},
                    .depth_stencil_format = granit::texture_format::d32_float,
                    .samples = granit::sample_count::one,
                    .vertex_buffers = vertex_layouts,
                    .primitive = {},
                    .depth = granit::depth_state{.test_enabled = true, .write_enabled = true},
                    .color_blends = {},
                    .depth_bias = std::nullopt});
  }

  granit::result render_frame() noexcept {
    granit::acquired_frame frame;
    auto result = swapchain_.acquire(frame);
    if (result.failed())
      return result;
    recreate_ = recreate_ || frame.needs_recreate();

    granit::swapchain_backbuffer backbuffer;
    result = swapchain_.backbuffer(frame, backbuffer);
    granit::frame_recording recording;
    if (result.ok())
      result = frame_context_.begin(frame, recording);

    const auto aspect =
        static_cast<float>(swapchain_info_.width) / static_cast<float>(swapchain_info_.height);
    const auto matrix =
        make_model_view_projection(static_cast<float>(rendered_frames_) * 0.02F, aspect);
    const auto uniform_offset = uniform_stride_ * recording.frame_slot();
    if (result.ok())
      result = uniform_buffer_.write(uniform_offset, std::as_bytes(std::span{&matrix, 1}));

    const granit::viewport viewport{
        0, 0, static_cast<float>(swapchain_info_.width), static_cast<float>(swapchain_info_.height),
        0, 1};
    const granit::scissor scissor{0, 0, swapchain_info_.width, swapchain_info_.height};
    const granit::color_attachment_desc color{
        .view = backbuffer.view,
        .resolve_view = {},
        .clear_value = {.red = 0.04F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
    const granit::depth_stencil_attachment_desc depth{.view = depth_view_.ref(),
                                                      .clear_value = {.depth = 1.0F}};
    const granit::rendering_desc rendering{
        .color_attachments = std::span{&color, 1},
        .depth_stencil_attachment = &depth,
        .area = {0, 0, swapchain_info_.width, swapchain_info_.height}};
    auto& recorder = recording.recorder();
    if (result.ok())
      result = recorder.set_viewports(0, std::span{&viewport, 1});
    if (result.ok())
      result = recorder.set_scissors(0, std::span{&scissor, 1});
    if (result.ok())
      result = recorder.bind_graphics_pipeline(pipeline_);
    if (result.ok())
      result = mesh_.bind(recorder);
    const std::array dynamic_offsets{static_cast<std::uint32_t>(uniform_offset)};
    if (result.ok())
      result = recorder.bind_graphics_group(layout_, 0, resource_group_, dynamic_offsets);
    if (result.ok())
      result = recorder.begin_rendering(rendering);
    if (result.ok())
      result = mesh_.draw(recorder);
    if (result.ok())
      result = recorder.end_rendering();
    if (result.ok())
      result = recording.submit();
    if (result.ok())
      result = swapchain_.present(frame);

    recreate_ = recreate_ || frame.needs_recreate();
    if (result.failed()) {
      if (recording.valid())
        static_cast<void>(recording.abort());
      if (frame.valid())
        static_cast<void>(swapchain_.cancel(frame));
    }
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
  granit::frame_context frame_context_;
  std::vector<std::byte> shader_archive_;
  granit::shader_library shader_library_;
  granit::shader vertex_shader_;
  granit::shader fragment_shader_;
  granit::texture checker_texture_;
  granit::texture_view checker_view_;
  granit::sampler checker_sampler_;
  granit::buffer vertex_buffer_;
  granit::buffer index_buffer_;
  granit::mesh mesh_;
  granit::buffer uniform_buffer_;
  granit::texture depth_texture_;
  granit::texture_view depth_view_;
  granit::bind_group_layout resource_layout_;
  granit::pipeline_layout layout_;
  granit::bind_group resource_group_;
  granit::graphics_pipeline pipeline_;
  granit::window_state window_state_{};
  granit::swapchain_info swapchain_info_{};
  application_phase phase_{application_phase::renderer_initializing};
  bool running_{true};
  bool recreate_{};
  bool smoke_test_{};
  std::uint64_t uniform_stride_{};
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
