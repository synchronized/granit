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

constexpr std::uint32_t maximum_instance_dimension = 32;
constexpr std::uint32_t maximum_instance_count =
    maximum_instance_dimension * maximum_instance_dimension;

struct instance_data {
  std::array<float, 4> offset_scale;
  std::array<float, 4> color;
};

struct scene_uniforms {
  matrix4 view_projection;
  std::array<float, 4> animation{};
};

static_assert(sizeof(instance_data) == sizeof(float) * 8);
static_assert(sizeof(scene_uniforms) == sizeof(float) * 20);

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
  [[nodiscard]] std::uint32_t instances() const noexcept { return instance_count_; }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return runtime_.canvas_items(); }

private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.failed())
      return result;
    initialize_instances();
    result = create_depth_target();
    if (result.ok())
      result = frame_context_.initialize(renderer());
    if (result.ok())
      result = shader_library_.initialize(renderer(), tutorial_instancing::shader_archive());
    if (result.ok())
      result = shader_library_.create_shader("instancing.vertex", vertex_shader_);
    if (result.ok())
      result = shader_library_.create_shader("instancing.fragment", fragment_shader_);
    if (result.ok())
      result = initialize_buffers();
    if (result.ok())
      result = initialize_pipeline_layout();
    if (result.ok())
      result = create_pipeline();
    if (result.ok() &&
        !camera_.focus(instance_bounds(), presentation_info().width, presentation_info().height))
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
    runtime_.shutdown();
    static_cast<void>(pipeline_.reset());
    static_cast<void>(resource_group_.reset());
    static_cast<void>(pipeline_layout_.reset());
    static_cast<void>(resource_layout_.reset());
    static_cast<void>(uniform_buffer_.reset());
    static_cast<void>(mesh_.reset());
    static_cast<void>(instance_buffer_.reset());
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

  void initialize_instances() noexcept {
    instance_count_ = rows_ * columns_;
    for (std::uint32_t row = 0; row < rows_; ++row) {
      for (std::uint32_t column = 0; column < columns_; ++column) {
        const auto index = row * columns_ + column;
        const float x =
            (static_cast<float>(column) - (static_cast<float>(columns_) - 1) * 0.5F) * spacing_;
        const float y =
            (static_cast<float>(row) - (static_cast<float>(rows_) - 1) * 0.5F) * spacing_;
        const float z = ((column + row) % 3 == 0) ? -0.8F : 0.0F;
        const float red = 0.25F + 0.7F * static_cast<float>(column) /
                                      static_cast<float>(std::max(columns_ - 1, 1U));
        const float green =
            0.25F + 0.65F * static_cast<float>(row) / static_cast<float>(std::max(rows_ - 1, 1U));
        const float blue = 0.95F - 0.5F * static_cast<float>(column + row) /
                                       static_cast<float>(std::max(columns_ + rows_ - 2, 1U));
        instances_[index] = {
            .offset_scale = {x, y, z, 0.48F},
            .color = {red, green, blue, 1.0F},
        };
      }
    }
  }

  [[nodiscard]] granit::example::camera::camera_bounds instance_bounds() const noexcept {
    const auto width = static_cast<float>(columns_ - 1) * spacing_ + 2.0F;
    const auto height = static_cast<float>(rows_ - 1) * spacing_ + 2.0F;
    return {.radius = std::sqrt(width * width + height * height) * 0.5F};
  }

  granit::result initialize_buffers() noexcept {
    auto result = vertex_buffer_.initialize(
        renderer_owner(),
        {.size = sizeof(tutorial_instancing::vertices),
         .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination});
    if (result.ok()) {
      result = index_buffer_.initialize(
          renderer_owner(),
          {.size = sizeof(tutorial_instancing::indices),
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
      uniform_stride_ = align_up(sizeof(scene_uniforms), limits.uniform_buffer_offset_alignment);
      result = uniform_buffer_.initialize(
          renderer_owner(),
          {.size = uniform_stride_ * GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
           .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination});
    }

    granit::upload_batch upload;
    if (result.ok())
      result = upload.initialize(renderer_owner());
    if (result.ok()) {
      result = upload.write_buffer(vertex_buffer_.ref(), 0,
                                   std::as_bytes(std::span{tutorial_instancing::vertices}));
    }
    if (result.ok()) {
      result = upload.write_buffer(index_buffer_.ref(), 0,
                                   std::as_bytes(std::span{tutorial_instancing::indices}));
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
    };
    const std::array instance_attributes{
        granit::vertex_attribute{.location = 1, .format = granit::vertex_format::float32x4},
        granit::vertex_attribute{
            .location = 2, .format = granit::vertex_format::float32x4, .offset = sizeof(float) * 4},
    };
    const std::array bindings{
        granit::mesh_vertex_buffer{.buffer = vertex_buffer_.ref(),
                                   .layout = {.stride = sizeof(tutorial_instancing::vertex),
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
                                                   tutorial_instancing::indices.size()),
                                               .instance_count = instance_count_});
  }

  granit::result initialize_pipeline_layout() noexcept {
    const std::array entries{
        granit::bind_group_layout_entry{.binding = 0,
                                        .type = granit::binding_type::dynamic_uniform_buffer,
                                        .visibility = granit::shader_stage_flags::vertex},
    };
    auto result = resource_layout_.initialize(renderer_owner(), entries);
    const std::array layouts{resource_layout_.ref()};
    if (result.ok())
      result = pipeline_layout_.initialize(renderer_owner(), layouts);
    const std::array resources{
        granit::bind_group_entry{
            .binding = 0, .resource = uniform_buffer_.ref(), .size = sizeof(scene_uniforms)},
    };
    if (result.ok())
      result = resource_group_.initialize(renderer_owner(), resource_layout_, resources);
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

  granit::result create_pipeline() noexcept {
    auto result = pipeline_.reset();
    if (result.failed())
      return result;
    const auto format = presentation_info().format;
    const std::array vertex_attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
    };
    const std::array instance_attributes{
        granit::vertex_attribute{.location = 1, .format = granit::vertex_format::float32x4},
        granit::vertex_attribute{
            .location = 2, .format = granit::vertex_format::float32x4, .offset = sizeof(float) * 4},
    };
    const std::array vertex_layouts{
        granit::vertex_buffer_layout{.stride = sizeof(tutorial_instancing::vertex),
                                     .attributes = vertex_attributes},
        granit::vertex_buffer_layout{.stride = sizeof(instance_data),
                                     .step_mode = granit::vertex_step_mode::instance,
                                     .attributes = instance_attributes},
    };
    result = pipeline_.initialize(
        renderer_owner(),
        {.layout = pipeline_layout_.ref(),
         .vertex_shader = vertex_shader_.ref(),
         .fragment_shader = fragment_shader_.ref(),
         .color_formats = std::span{&format, 1},
         .depth_stencil_format = granit::texture_format::d32_float,
         .vertex_buffers = vertex_layouts,
         .primitive = {.cull = granit::cull_mode::back},
         .depth = granit::depth_state{.test_enabled = true, .write_enabled = true},
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
      result =
          runtime_.begin_frame(window_state, frame.delta_seconds,
                               {.name = "03 Instancing",
                                .description = "Dynamic instance range and draw-call comparison",
                                .frame = rendered_frames()});
    }
    int rows = static_cast<int>(rows_);
    int columns = static_cast<int>(columns_);
    bool layout_changed{};
    if (result.ok()) {
      layout_changed = ImGui::SliderInt("Rows", &rows, 1, maximum_instance_dimension);
      layout_changed =
          ImGui::SliderInt("Columns", &columns, 1, maximum_instance_dimension) || layout_changed;
      layout_changed = ImGui::SliderFloat("Spacing", &spacing_, 0.8F, 3.0F) || layout_changed;
      ImGui::Checkbox("Animate", &animate_);
      ImGui::SliderFloat("Animation speed", &animation_speed_, 0.0F, 3.0F);
      ImGui::Checkbox("Instanced draw", &use_instancing_);
      ImGui::Checkbox("Auto orbit", &auto_orbit_);
      if (ImGui::Button("Reset camera"))
        camera_.reset();
      ImGui::Text("Instances: %u / %u", instance_count_, maximum_instance_count);
      ImGui::Text("Draw calls: %u", use_instancing_ ? 1U : instance_count_);
      result = runtime_.end_frame();
    }
    if (result.failed())
      return result;
    if (layout_changed) {
      rows_ = static_cast<std::uint32_t>(rows);
      columns_ = static_cast<std::uint32_t>(columns);
      initialize_instances();
      if (!camera_.focus(instance_bounds(), frame.swapchain.width, frame.swapchain.height))
        return granit::result::invalid_argument;
    }

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
    scene_uniforms uniforms{};
    uniforms.view_projection = matrices.view_projection;
    uniforms.animation[0] = time_;
    const auto uniform_offset = uniform_stride_ * recording.frame_slot();
    if (result.ok()) {
      result = instance_buffer_.write(
          0,
          std::as_bytes(std::span{instances_.data(), static_cast<std::size_t>(instance_count_)}));
    }
    if (result.ok())
      result = uniform_buffer_.write(uniform_offset, std::as_bytes(std::span{&uniforms, 1}));

    const granit::viewport viewport{
        0, 0, static_cast<float>(frame.swapchain.width), static_cast<float>(frame.swapchain.height),
        0, 1};
    const granit::scissor scissor{0, 0, frame.swapchain.width, frame.swapchain.height};
    const granit::color_attachment_desc color{
        .view = frame.backbuffer.view,
        .resolve_view = {},
        .clear_value = {.red = 0.025F, .green = 0.035F, .blue = 0.07F, .alpha = 1.0F}};
    const granit::depth_stencil_attachment_desc depth{.view = depth_view_.ref(),
                                                      .clear_value = {.depth = 1.0F}};
    const granit::rendering_desc rendering{
        .color_attachments = std::span{&color, 1},
        .depth_stencil_attachment = &depth,
        .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
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
      result = recorder.bind_graphics_group(pipeline_layout_, 0, resource_group_, dynamic_offsets);
    if (result.ok())
      result = recorder.begin_rendering(rendering);
    if (result.ok() && use_instancing_)
      result = mesh_.draw(recorder, {.instance_count = instance_count_});
    if (result.ok() && !use_instancing_) {
      for (std::uint32_t index = 0; index < instance_count_ && result.ok(); ++index)
        result = mesh_.draw(recorder, {.instance_count = 1, .first_instance = index});
    }
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

  std::array<instance_data, maximum_instance_count> instances_{};
  granit::example::tutorial::tutorial_runtime runtime_;
  granit::example::camera::orbit_camera camera_;
  granit::example::camera::orbit_camera_input_accumulator camera_input_;
  granit::frame_context frame_context_;
  granit::shader_library shader_library_;
  granit::shader vertex_shader_;
  granit::shader fragment_shader_;
  granit::buffer vertex_buffer_;
  granit::buffer index_buffer_;
  granit::buffer instance_buffer_;
  granit::mesh mesh_;
  granit::buffer uniform_buffer_;
  granit::texture depth_texture_;
  granit::texture_view depth_view_;
  granit::bind_group_layout resource_layout_;
  granit::pipeline_layout pipeline_layout_;
  granit::bind_group resource_group_;
  granit::graphics_pipeline pipeline_;
  granit::texture_format pipeline_format_{granit::texture_format::undefined};
  std::uint64_t uniform_stride_{};
  float time_{};
  float spacing_{1.65F};
  float animation_speed_{1.0F};
  std::uint32_t rows_{5};
  std::uint32_t columns_{9};
  std::uint32_t instance_count_{45};
  bool animate_{true};
  bool auto_orbit_{};
  bool use_instancing_{true};
  bool smoke_test_{};
};

tutorial_application application;

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_03_rendered_frames() noexcept {
  return application.rendered_frames();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_03_recreate_count() noexcept {
  return application.completed_recreates();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_03_feature_value() noexcept {
  return application.instances();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_03_canvas_items() noexcept {
  return application.canvas_items();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_03_ready() noexcept {
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
                       .title = "Granit Instancing",
                       .renderer = {.application_name = "Granit Instancing",
                                    .presentation = granit::presentation_mode::enabled},
                       .swapchain = {},
                       .smoke_test = smoke_test});
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  if (result.failed())
    return report_failure("application run", result);
  return 0;
}
