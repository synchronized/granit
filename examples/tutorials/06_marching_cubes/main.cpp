// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "camera/orbit_camera.h"
#include "camera/orbit_camera_input_accumulator.h"
#include "marching_cubes_table.h"
#include "shader_archive.h"
#include "tutorial/tutorial_runtime.h"

#include <granit/math/functions.hpp>
#include <granit/renderer/readback_batch.hpp>
#include <granit/renderer/timestamp_query.hpp>
#include <imgui.h>

#include <algorithm>
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

using granit::math::matrix4;

constexpr std::uint32_t workgroup_size = 4;
constexpr std::uint32_t maximum_grid_size = 32;
constexpr std::uint64_t generated_vertex_size = sizeof(float) * 8;

struct scene_uniforms {
  matrix4 view_projection;
  std::array<float, 4> grid_iso_time_capacity;
  std::array<float, 4> metaball_parameters;
};

struct generation_state {
  std::uint32_t vertex_count{};
  std::uint32_t overflow{};
  std::uint32_t reserved[2]{};
  granit::draw_indirect_args draw{};
};

static_assert(sizeof(scene_uniforms) == sizeof(float) * 24);
static_assert(sizeof(generation_state) == 32);

std::uint64_t align_up(std::uint64_t value, std::uint64_t alignment) {
  return alignment == 0 ? value : (value + alignment - 1) / alignment * alignment;
}

std::uint64_t cube(std::uint32_t value) {
  return static_cast<std::uint64_t>(value) * value * value;
}

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

class tutorial_application final : public granit::example::application {
public:
  void set_smoke_test(bool enabled) noexcept { smoke_test_ = enabled; }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return runtime_.canvas_items(); }
  [[nodiscard]] std::uint32_t generated_vertices() const noexcept {
    return last_generation_.draw.vertex_count;
  }
  [[nodiscard]] granit_result shutdown_result() const noexcept { return shutdown_result_; }

private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.ok())
      result = frame_context_.initialize(renderer());
    if (result.ok())
      result = shader_library_.initialize(renderer(), tutorial_marching_cubes::shader_archive());
    if (result.ok())
      result = shader_library_.create_shader("marching_cubes.density", density_shader_);
    if (result.ok())
      result = shader_library_.create_shader("marching_cubes.polygonize", polygonize_shader_);
    if (result.ok())
      result = shader_library_.create_shader("marching_cubes.finalize", finalize_shader_);
    if (result.ok())
      result = shader_library_.create_shader("marching_cubes.vertex", vertex_shader_);
    if (result.ok())
      result = shader_library_.create_shader("marching_cubes.fragment", fragment_shader_);
    if (result.ok())
      result = initialize_persistent_buffers();
    if (result.ok())
      result = initialize_timestamps();
    if (result.ok())
      result =
          readback_batch_.create(renderer_owner(), {.max_result_bytes = sizeof(generation_state),
                                                    .max_operation_count = 1});
    if (result.ok())
      result = initialize_layouts_and_pipelines();
    if (result.ok())
      result = create_depth_target();
    if (result.ok())
      result = create_graphics_pipeline();
    if (result.ok() &&
        !camera_.focus({.radius = 2.2F}, presentation_info().width, presentation_info().height))
      result = granit::result::invalid_argument;
    return result;
  }

  granit::result on_swapchain_changed(const granit::swapchain_info& info) noexcept override {
    auto result = create_depth_target();
    if (result.ok() && info.format != pipeline_format_)
      result = create_graphics_pipeline();
    return result;
  }

  void on_shutdown(granit::result reason) noexcept override {
    shutdown_result_ = reason.native();
    runtime_.shutdown();
    static_cast<void>(readback_operation_.reset());
    static_cast<void>(readback_batch_.reset_handle());
    static_cast<void>(graphics_pipeline_.reset());
    static_cast<void>(finalize_pipeline_.reset());
    static_cast<void>(polygonize_pipeline_.reset());
    static_cast<void>(density_pipeline_.reset());
    static_cast<void>(graphics_group_.reset());
    static_cast<void>(graphics_pipeline_layout_.reset());
    static_cast<void>(graphics_group_layout_.reset());
    static_cast<void>(compute_pipeline_layout_.reset());
    static_cast<void>(compute_group_layout_.reset());
    static_cast<void>(depth_view_.reset());
    static_cast<void>(depth_texture_.reset());
    static_cast<void>(timestamp_queries_.reset());
    for (auto& buffer : readback_buffers_)
      static_cast<void>(buffer.reset());
    static_cast<void>(table_buffer_.reset());
    static_cast<void>(uniform_buffer_.reset());
    static_cast<void>(fragment_shader_.reset());
    static_cast<void>(vertex_shader_.reset());
    static_cast<void>(finalize_shader_.reset());
    static_cast<void>(polygonize_shader_.reset());
    static_cast<void>(density_shader_.reset());
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

  granit::result initialize_persistent_buffers() noexcept {
    granit::renderer_limits limits;
    auto result = renderer_owner().get_limits(limits);
    if (result.ok()) {
      uniform_stride_ = align_up(sizeof(scene_uniforms), limits.uniform_buffer_offset_alignment);
      result = uniform_buffer_.initialize(
          renderer_owner(),
          {.size = uniform_stride_ * GRANIT_MAX_FRAMES_IN_FLIGHT,
           .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination});
    }
    if (result.ok()) {
      result = table_buffer_.initialize(
          renderer_owner(),
          {.size = sizeof(tutorial_marching_cubes::triangle_table),
           .usage = granit::buffer_usage::storage | granit::buffer_usage::transfer_destination},
          std::as_bytes(std::span{tutorial_marching_cubes::triangle_table}));
    }
    for (auto& buffer : readback_buffers_) {
      if (result.ok()) {
        result = buffer.initialize(renderer_owner(),
                                   {.size = sizeof(generation_state),
                                    .usage = granit::buffer_usage::transfer_source |
                                             granit::buffer_usage::transfer_destination,
                                    .location = granit::memory_location::device});
      }
    }
    return result;
  }

  granit::result initialize_timestamps() noexcept {
    granit::renderer_limits limits;
    auto result = renderer_owner().get_limits(limits);
    timestamps_enabled_ = result.ok() && limits.supports_timestamp_queries();
    if (result.ok() && timestamps_enabled_) {
      result = timestamp_queries_.initialize(renderer_owner(),
                                             GRANIT_MAX_FRAMES_IN_FLIGHT * timestamps_per_frame);
    }
    return result;
  }

  granit::result initialize_layouts_and_pipelines() noexcept {
    const auto compute_visibility = granit::shader_stage_flags::compute;
    const std::array compute_entries{
        granit::bind_group_layout_entry{.binding = 0,
                                        .type = granit::binding_type::dynamic_uniform_buffer,
                                        .visibility = compute_visibility},
        granit::bind_group_layout_entry{.binding = 1,
                                        .type = granit::binding_type::storage_buffer,
                                        .visibility = compute_visibility},
        granit::bind_group_layout_entry{.binding = 2,
                                        .type = granit::binding_type::storage_buffer,
                                        .visibility = compute_visibility},
        granit::bind_group_layout_entry{.binding = 3,
                                        .type = granit::binding_type::storage_buffer,
                                        .visibility = compute_visibility},
        granit::bind_group_layout_entry{.binding = 4,
                                        .type = granit::binding_type::storage_buffer,
                                        .visibility = compute_visibility},
    };
    auto result = compute_group_layout_.initialize(renderer_owner(), compute_entries);
    const std::array compute_layouts{compute_group_layout_.ref()};
    if (result.ok())
      result = compute_pipeline_layout_.initialize(renderer_owner(), compute_layouts);
    if (result.ok())
      result =
          density_pipeline_.initialize(renderer_owner(), {.layout = compute_pipeline_layout_.ref(),
                                                          .compute_shader = density_shader_.ref()});
    if (result.ok())
      result = polygonize_pipeline_.initialize(
          renderer_owner(),
          {.layout = compute_pipeline_layout_.ref(), .compute_shader = polygonize_shader_.ref()});
    if (result.ok())
      result = finalize_pipeline_.initialize(
          renderer_owner(),
          {.layout = compute_pipeline_layout_.ref(), .compute_shader = finalize_shader_.ref()});

    const std::array graphics_entries{
        granit::bind_group_layout_entry{.binding = 0,
                                        .type = granit::binding_type::dynamic_uniform_buffer,
                                        .visibility = granit::shader_stage_flags::vertex},
    };
    if (result.ok())
      result = graphics_group_layout_.initialize(renderer_owner(), graphics_entries);
    const std::array graphics_layouts{graphics_group_layout_.ref()};
    if (result.ok())
      result = graphics_pipeline_layout_.initialize(renderer_owner(), graphics_layouts);
    const std::array graphics_resources{
        granit::bind_group_entry{
            .binding = 0, .resource = uniform_buffer_.ref(), .size = sizeof(scene_uniforms)},
    };
    if (result.ok()) {
      result =
          graphics_group_.initialize(renderer_owner(), graphics_group_layout_, graphics_resources);
    }
    return result;
  }

  granit::result create_depth_target() noexcept {
    auto result = depth_view_.reset();
    if (result.ok())
      result = depth_texture_.reset();
    if (result.ok()) {
      result = depth_texture_.initialize(renderer_owner(),
                                         {.format = granit::texture_format::d32_float,
                                          .usage = granit::texture_usage::depth_stencil_attachment,
                                          .width = presentation_info().width,
                                          .height = presentation_info().height});
    }
    if (result.ok())
      result = depth_view_.initialize(renderer_owner(), depth_texture_);
    return result;
  }

  granit::result create_graphics_pipeline() noexcept {
    auto result = graphics_pipeline_.reset();
    const std::array attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x4},
        granit::vertex_attribute{
            .location = 1, .format = granit::vertex_format::float32x4, .offset = sizeof(float) * 4},
    };
    const std::array layouts{
        granit::vertex_buffer_layout{.stride = generated_vertex_size, .attributes = attributes},
    };
    const auto format = presentation_info().format;
    if (result.ok()) {
      result = graphics_pipeline_.initialize(
          renderer_owner(),
          {.layout = graphics_pipeline_layout_.ref(),
           .vertex_shader = vertex_shader_.ref(),
           .fragment_shader = fragment_shader_.ref(),
           .color_formats = std::span{&format, 1},
           .depth_stencil_format = granit::texture_format::d32_float,
           .vertex_buffers = layouts,
           .primitive = {.cull = granit::cull_mode::none},
           .depth = granit::depth_state{.test_enabled = true, .write_enabled = true},
           .color_blends = {},
           .depth_bias = std::nullopt});
    }
    if (result.ok())
      pipeline_format_ = format;
    return result;
  }

  granit::result update_readback(std::uint32_t slot) noexcept {
    if (readback_operation_.valid()) {
      granit::async_operation_status status;
      auto result = readback_operation_.get_status(status);
      if (result.failed())
        return result;
      if (status.complete()) {
        if (status.state == granit::async_operation_state::succeeded) {
          std::uint64_t size = sizeof(last_generation_);
          result = granit::copy_readback_result(
              readback_operation_, 0, std::as_writable_bytes(std::span{&last_generation_, 1}),
              size);
          if (result.failed())
            return result;
        } else if (status.operation_result.failed()) {
          return status.operation_result;
        }
        static_cast<void>(readback_operation_.reset());
        result = readback_batch_.reset();
        if (result.failed())
          return result;
        readback_pending_slot_.reset();
      }
    }

    if (readback_operation_.valid() || slot >= readback_buffers_.size() || !readback_valid_[slot])
      return granit::result::success;
    std::uint32_t result_index{};
    auto result = readback_batch_.read_buffer(readback_buffers_[slot].ref(), 0,
                                              sizeof(generation_state), result_index);
    if (result.ok())
      result = readback_batch_.submit_async(readback_operation_);
    if (result.ok()) {
      readback_valid_[slot] = false;
      readback_pending_slot_ = slot;
    }
    return result;
  }

  void update_timestamps(std::uint32_t slot) noexcept {
    if (!timestamps_enabled_ || slot >= timestamp_valid_.size() || !timestamp_valid_[slot])
      return;
    std::array<std::uint64_t, timestamps_per_frame> values{};
    if (timestamp_queries_.get_results(slot * timestamps_per_frame, values).ok()) {
      for (std::size_t index = 0; index < gpu_times_ms_.size(); ++index) {
        gpu_times_ms_[index] = static_cast<double>(values[index + 1] - values[index]) / 1'000'000.0;
      }
    }
  }

  granit::result draw_panel(granit::example::present_frame& frame) noexcept {
    granit::window_state window_state;
    auto result = app_window().get_state(window_state);
    if (result.ok()) {
      result = runtime_.begin_frame(window_state, frame.delta_seconds,
                                    {.name = "06 Marching Cubes",
                                     .description = "GPU density, polygonize and indirect draw",
                                     .frame = rendered_frames()});
    }
    int grid_size = static_cast<int>(grid_size_);
    if (result.ok()) {
      ImGui::SliderInt("Grid size", &grid_size, 8, maximum_grid_size);
      ImGui::SliderFloat("ISO", &iso_, 0.1F, 2.0F);
      ImGui::SliderInt("Ball count", &ball_count_, 1, 5);
      ImGui::SliderFloat("Radius", &radius_, 0.2F, 1.0F);
      ImGui::SliderFloat("Field strength", &strength_, 0.25F, 2.0F);
      ImGui::Checkbox("Animate", &animate_);
      ImGui::SliderFloat("Animation speed", &animation_speed_, 0.0F, 3.0F);
      ImGui::Checkbox("Auto orbit", &auto_orbit_);
      if (ImGui::Button("Reset camera"))
        camera_.reset();
      ImGui::Text("Vertices: %u / %u", last_generation_.draw.vertex_count, vertex_capacity());
      ImGui::Text("Triangles: %u", last_generation_.draw.vertex_count / 3);
      ImGui::Text("Overflow: %s", last_generation_.overflow != 0 ? "yes" : "no");
      if (timestamps_enabled_) {
        ImGui::Text("GPU ms: density %.3f, polygonize %.3f", gpu_times_ms_[0], gpu_times_ms_[1]);
        ImGui::Text("GPU ms: finalize %.3f, draw %.3f", gpu_times_ms_[2], gpu_times_ms_[3]);
      } else {
        ImGui::TextDisabled("GPU timestamps: unavailable");
      }
      ImGui::TextDisabled("Wireframe: unsupported by portable profile");
      result = runtime_.end_frame();
    }
    grid_size_ = static_cast<std::uint32_t>(grid_size);
    return result;
  }

  [[nodiscard]] std::uint32_t vertex_capacity() const noexcept {
    const auto cells = cube(grid_size_ - 1);
    const auto theoretical = std::min<std::uint64_t>(cells * 15, UINT32_MAX);
    return static_cast<std::uint32_t>(theoretical);
  }

  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    camera_input_.begin_frame();
    auto result = draw_panel(frame);
    granit::frame_recording recording;
    if (result.ok())
      result = frame_context_.begin(frame.acquired, recording);
    if (result.failed())
      return result;
    result = update_readback(recording.frame_slot());
    update_timestamps(recording.frame_slot());
    // WebGPU 映射异步完成；映射期间不能把同一帧槽的 staging buffer 再作为 GPU copy 目标。
    const bool readback_destination_available =
        !readback_pending_slot_.has_value() || *readback_pending_slot_ != recording.frame_slot();

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

    const auto capacity = vertex_capacity();
    const scene_uniforms uniforms{
        .view_projection = matrices.view_projection,
        .grid_iso_time_capacity = {static_cast<float>(grid_size_), iso_, time_,
                                   static_cast<float>(capacity)},
        .metaball_parameters = {static_cast<float>(ball_count_), radius_, strength_, 0.0F},
    };
    const auto uniform_offset = uniform_stride_ * recording.frame_slot();
    if (result.ok())
      result = uniform_buffer_.write(uniform_offset, std::as_bytes(std::span{&uniforms, 1}));

    granit::transient_buffer_slice field;
    granit::transient_buffer_slice vertices;
    granit::transient_buffer_slice state;
    if (result.ok()) {
      result = recording.allocate_transient_buffer({.size = cube(grid_size_) * sizeof(float) * 4,
                                                    .usage = granit::buffer_usage::storage,
                                                    .location = granit::memory_location::device},
                                                   field);
    }
    if (result.ok()) {
      result = recording.allocate_transient_buffer(
          {.size = static_cast<std::uint64_t>(capacity) * generated_vertex_size,
           .usage = granit::buffer_usage::storage | granit::buffer_usage::vertex,
           .location = granit::memory_location::device},
          vertices);
    }
    if (result.ok()) {
      result = recording.allocate_transient_buffer(
          {.size = sizeof(generation_state),
           .usage = granit::buffer_usage::storage | granit::buffer_usage::indirect |
                    granit::buffer_usage::transfer_source |
                    granit::buffer_usage::transfer_destination,
           .location = granit::memory_location::device},
          state);
    }

    granit::bind_group compute_group;
    const std::array compute_resources{
        granit::bind_group_entry{
            .binding = 0, .resource = uniform_buffer_.ref(), .size = sizeof(scene_uniforms)},
        granit::bind_group_entry{
            .binding = 1, .resource = field.buffer, .offset = field.offset, .size = field.size},
        granit::bind_group_entry{.binding = 2,
                                 .resource = table_buffer_.ref(),
                                 .size = sizeof(tutorial_marching_cubes::triangle_table)},
        granit::bind_group_entry{.binding = 3,
                                 .resource = vertices.buffer,
                                 .offset = vertices.offset,
                                 .size = vertices.size},
        granit::bind_group_entry{
            .binding = 4, .resource = state.buffer, .offset = state.offset, .size = state.size},
    };
    if (result.ok())
      result = compute_group.initialize(renderer_owner(), compute_group_layout_, compute_resources);

    auto& recorder = recording.recorder();
    const std::array dynamic_offsets{static_cast<std::uint32_t>(uniform_offset)};
    const auto first_timestamp = recording.frame_slot() * timestamps_per_frame;
    if (result.ok() && timestamps_enabled_)
      result = recorder.reset_timestamp_queries(timestamp_queries_, first_timestamp,
                                                timestamps_per_frame);
    if (result.ok() && timestamps_enabled_)
      result = recorder.write_timestamp(timestamp_queries_, granit::timestamp_stage::top,
                                        first_timestamp);
    if (result.ok())
      result = recorder.fill_buffer(state.buffer, state.offset, state.size, 0);
    if (result.ok())
      result = recorder.bind_compute_pipeline(density_pipeline_);
    if (result.ok())
      result =
          recorder.bind_compute_group(compute_pipeline_layout_, 0, compute_group, dynamic_offsets);
    const auto sample_groups = (grid_size_ + workgroup_size - 1) / workgroup_size;
    const auto cell_groups = (grid_size_ - 1 + workgroup_size - 1) / workgroup_size;
    if (result.ok())
      result = recorder.dispatch(sample_groups, sample_groups, sample_groups);
    if (result.ok() && timestamps_enabled_)
      result = recorder.write_timestamp(timestamp_queries_, granit::timestamp_stage::bottom,
                                        first_timestamp + 1);
    if (result.ok())
      result = recorder.bind_compute_pipeline(polygonize_pipeline_);
    if (result.ok())
      result = recorder.dispatch(cell_groups, cell_groups, cell_groups);
    if (result.ok() && timestamps_enabled_)
      result = recorder.write_timestamp(timestamp_queries_, granit::timestamp_stage::bottom,
                                        first_timestamp + 2);
    if (result.ok())
      result = recorder.bind_compute_pipeline(finalize_pipeline_);
    if (result.ok())
      result = recorder.dispatch(1, 1, 1);
    if (result.ok() && timestamps_enabled_)
      result = recorder.write_timestamp(timestamp_queries_, granit::timestamp_stage::bottom,
                                        first_timestamp + 3);

    const granit::buffer_copy_region state_copy{
        .source_offset = state.offset,
        .destination_offset = 0,
        .size = sizeof(generation_state),
    };
    if (result.ok() && readback_destination_available) {
      result = recorder.copy_buffer(state.buffer, readback_buffers_[recording.frame_slot()].ref(),
                                    std::span{&state_copy, 1});
    }

    const granit::viewport viewport{
        0, 0, static_cast<float>(frame.swapchain.width), static_cast<float>(frame.swapchain.height),
        0, 1};
    const granit::scissor scissor{0, 0, frame.swapchain.width, frame.swapchain.height};
    const granit::vertex_buffer_binding vertex_binding{vertices.buffer, vertices.offset};
    const granit::color_attachment_desc color{
        .view = frame.backbuffer.view,
        .resolve_view = {},
        .clear_value = {.red = 0.01F, .green = 0.015F, .blue = 0.04F, .alpha = 1.0F}};
    const granit::depth_stencil_attachment_desc depth{.view = depth_view_.ref(),
                                                      .clear_value = {.depth = 1.0F}};
    const granit::rendering_desc rendering{
        .color_attachments = std::span{&color, 1},
        .depth_stencil_attachment = &depth,
        .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
    if (result.ok())
      result = recorder.set_viewports(0, std::span{&viewport, 1});
    if (result.ok())
      result = recorder.set_scissors(0, std::span{&scissor, 1});
    if (result.ok())
      result = recorder.bind_graphics_pipeline(graphics_pipeline_);
    if (result.ok())
      result = recorder.bind_graphics_group(graphics_pipeline_layout_, 0, graphics_group_,
                                            dynamic_offsets);
    if (result.ok())
      result = recorder.bind_vertex_buffers(0, std::span{&vertex_binding, 1});
    if (result.ok())
      result = recorder.begin_rendering(rendering);
    if (result.ok())
      result = recorder.draw_indirect(state.buffer, state.offset + 16);
    if (result.ok())
      result = recorder.end_rendering();
    if (result.ok() && timestamps_enabled_)
      result = recorder.write_timestamp(timestamp_queries_, granit::timestamp_stage::bottom,
                                        first_timestamp + 4);
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
    if (result.ok()) {
      result = recording.submit();
      if (result.ok()) {
        if (readback_destination_available)
          readback_valid_[recording.frame_slot()] = true;
        timestamp_valid_[recording.frame_slot()] = timestamps_enabled_;
      }
    }
    if (result.failed() && recording.valid())
      static_cast<void>(recording.abort());
    return result;
  }

  granit::frame_context frame_context_;
  granit::example::tutorial::tutorial_runtime runtime_;
  granit::example::camera::orbit_camera camera_;
  granit::example::camera::orbit_camera_input_accumulator camera_input_;
  granit::shader_library shader_library_;
  granit::shader density_shader_;
  granit::shader polygonize_shader_;
  granit::shader finalize_shader_;
  granit::shader vertex_shader_;
  granit::shader fragment_shader_;
  granit::buffer uniform_buffer_;
  granit::buffer table_buffer_;
  std::array<granit::buffer, GRANIT_MAX_FRAMES_IN_FLIGHT> readback_buffers_;
  std::array<bool, GRANIT_MAX_FRAMES_IN_FLIGHT> readback_valid_{};
  std::optional<std::uint32_t> readback_pending_slot_;
  granit::readback_batch readback_batch_;
  granit::async_operation readback_operation_;
  static constexpr std::uint32_t timestamps_per_frame = 5;
  granit::timestamp_query_pool timestamp_queries_;
  std::array<bool, GRANIT_MAX_FRAMES_IN_FLIGHT> timestamp_valid_{};
  std::array<double, timestamps_per_frame - 1> gpu_times_ms_{};
  granit::texture depth_texture_;
  granit::texture_view depth_view_;
  granit::bind_group_layout compute_group_layout_;
  granit::pipeline_layout compute_pipeline_layout_;
  granit::compute_pipeline density_pipeline_;
  granit::compute_pipeline polygonize_pipeline_;
  granit::compute_pipeline finalize_pipeline_;
  granit::bind_group_layout graphics_group_layout_;
  granit::pipeline_layout graphics_pipeline_layout_;
  granit::bind_group graphics_group_;
  granit::graphics_pipeline graphics_pipeline_;
  granit::texture_format pipeline_format_{granit::texture_format::undefined};
  generation_state last_generation_{};
  std::uint64_t uniform_stride_{};
  std::uint32_t grid_size_{16};
  float iso_{0.75F};
  float time_{};
  float radius_{0.55F};
  float strength_{1.0F};
  float animation_speed_{1.0F};
  int ball_count_{5};
  bool animate_{true};
  bool auto_orbit_{};
  bool timestamps_enabled_{};
  bool smoke_test_{};
  granit_result shutdown_result_{GRANIT_SUCCESS};
};

tutorial_application application;

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_06_rendered_frames() noexcept {
  return application.rendered_frames();
}
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_06_recreate_count() noexcept {
  return application.completed_recreates();
}
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_06_feature_value() noexcept {
  return application.generated_vertices();
}
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_06_canvas_items() noexcept {
  return application.canvas_items();
}
extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_06_ready() noexcept {
  return application.ready() ? 1 : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE granit_result granit_tutorial_06_shutdown_result() noexcept {
  return application.shutdown_result();
}
#endif

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";
  const std::string_view executable_path =
      argument_count > 0 && arguments[0] != nullptr ? arguments[0] : "";
  application.set_smoke_test(smoke_test);
  const auto result =
      application.run({.executable_path = executable_path,
                       .title = "Granit Marching Cubes",
                       .renderer = {.application_name = "Granit Marching Cubes",
                                    .presentation = granit::presentation_mode::enabled},
                       .swapchain = {},
                       .smoke_test = smoke_test});
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  return result.failed() ? report_failure("application run", result) : 0;
}
