// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "camera/orbit_camera.h"
#include "camera/orbit_camera_input_accumulator.h"
#include "model_data.hpp"
#include "shader_archive.h"
#include "tutorial/tutorial_runtime.h"

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

struct instance_data {
  std::array<float, 4> transform;
  std::array<float, 4> color_mode;
};
struct scene_uniforms {
  matrix4 view_projection;
  std::array<float, 4> light_direction;
  std::array<float, 4> ambient;
  std::array<float, 4> debug;
};
static_assert(sizeof(scene_uniforms) == sizeof(float) * 28);
constexpr std::uint32_t object_count = 6;

std::array<float, 3> normalized_or_up(const std::array<float, 3>& value) noexcept {
  const auto length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
  if (length < 0.0001F)
    return {0.0F, 1.0F, 0.0F};
  return {value[0] / length, value[1] / length, value[2] / length};
}

void append_gizmo_line(std::array<granit::debug_draw_line, 64>& lines, std::size_t& count,
                       const std::array<float, 3>& start, const std::array<float, 3>& end,
                       std::uint32_t color, float width = 2.5F) noexcept {
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

void append_gizmo_square(std::array<granit::debug_draw_line, 64>& lines, std::size_t& count,
                         const std::array<float, 3>& center, float half_size,
                         std::uint32_t color) noexcept {
  const std::array<float, 3> top_left{center[0] - half_size, center[1], center[2] - half_size};
  const std::array<float, 3> top_right{center[0] + half_size, center[1], center[2] - half_size};
  const std::array<float, 3> bottom_right{center[0] + half_size, center[1], center[2] + half_size};
  const std::array<float, 3> bottom_left{center[0] - half_size, center[1], center[2] + half_size};
  append_gizmo_line(lines, count, top_left, top_right, color);
  append_gizmo_line(lines, count, top_right, bottom_right, color);
  append_gizmo_line(lines, count, bottom_right, bottom_left, color);
  append_gizmo_line(lines, count, bottom_left, top_left, color);
}

void append_gizmo_arrow(std::array<granit::debug_draw_line, 64>& lines, std::size_t& count,
                        const std::array<float, 3>& start, const std::array<float, 3>& vector,
                        float scale, std::uint32_t color) noexcept {
  const auto direction = normalized_or_up(vector);
  const std::array<float, 3> end{start[0] + direction[0] * scale,
                                 start[1] + direction[1] * scale,
                                 start[2] + direction[2] * scale};
  append_gizmo_line(lines, count, start, end, color);
  const auto side = normalized_or_up({-direction[1], direction[0], 0.0F});
  const std::array<float, 3> head_base{end[0] - direction[0] * 0.18F,
                                       end[1] - direction[1] * 0.18F,
                                       end[2] - direction[2] * 0.18F};
  append_gizmo_line(lines, count, end,
                    {head_base[0] + side[0] * 0.08F, head_base[1] + side[1] * 0.08F,
                     head_base[2] + side[2] * 0.08F}, color);
  append_gizmo_line(lines, count, end,
                    {head_base[0] - side[0] * 0.08F, head_base[1] - side[1] * 0.08F,
                     head_base[2] - side[2] * 0.08F}, color);
}

int failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

class application final : public granit::example::application {
public:
  void set_smoke_test(bool value) noexcept { smoke_test_ = value; }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return runtime_.canvas_items(); }

private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.ok()) result = debug_draw_.initialize(renderer_owner(), {.initial_line_capacity = 64});
    if (result.ok()) result = frame_context_.initialize(renderer());
    if (result.ok()) result = shaders_.initialize(renderer(), tutorial_transparency::shader_archive());
    if (result.ok()) result = shaders_.create_shader("transparency.vertex", vertex_shader_);
    if (result.ok()) result = shaders_.create_shader("transparency.fragment", fragment_shader_);
    if (result.ok()) result = initialize_scene();
    if (result.ok()) result = initialize_layout();
    if (result.ok()) result = create_pipeline();
    if (result.ok() && !camera_.focus({.radius = 4.5F}, presentation_info().width, presentation_info().height))
      result = granit::result::invalid_argument;
    return result;
  }

  granit::result initialize_scene() noexcept {
    instances_ = {{
        {{-1.45F, 0.0F, 0.0F, 0.72F}, {0.92F, 0.18F, 0.12F, 0.0F}},
        {{0.0F, 0.0F, -0.35F, 0.72F}, {0.16F, 0.82F, 0.98F, 1.0F}},
        {{1.45F, 0.0F, -0.7F, 0.72F}, {0.95F, 0.78F, 0.12F, 2.0F}},
        {{-0.72F, 1.35F, -0.5F, 0.52F}, {0.35F, 0.95F, 0.25F, 2.0F}},
        {{0.72F, 1.35F, -0.9F, 0.52F}, {0.8F, 0.25F, 0.95F, 2.0F}},
        {{0.0F, -1.35F, -0.2F, 0.52F}, {0.15F, 0.9F, 0.55F, 1.0F}},
    }};
    auto result = vertex_buffer_.initialize(renderer_owner(), {.size = sizeof(tutorial_transparency::vertices),
      .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination});
    if (result.ok()) result = index_buffer_.initialize(renderer_owner(), {.size = sizeof(tutorial_transparency::indices),
      .usage = granit::buffer_usage::index | granit::buffer_usage::transfer_destination});
    if (result.ok()) result = instance_buffer_.initialize(renderer_owner(), {.size = sizeof(instances_),
      .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination});
    granit::renderer_limits limits;
    if (result.ok()) result = renderer_owner().get_limits(limits);
    if (result.ok()) {
      const auto alignment = limits.uniform_buffer_offset_alignment;
      uniform_stride_ = (sizeof(scene_uniforms) + alignment - 1) / alignment * alignment;
      result = uniform_buffer_.initialize(renderer_owner(), {.size = uniform_stride_ * GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
        .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination});
    }
    granit::upload_batch upload;
    if (result.ok()) result = upload.initialize(renderer_owner());
    if (result.ok()) result = upload.write_buffer(vertex_buffer_.ref(), 0, std::as_bytes(std::span{tutorial_transparency::vertices}));
    if (result.ok()) result = upload.write_buffer(index_buffer_.ref(), 0, std::as_bytes(std::span{tutorial_transparency::indices}));
    draw_instances_ = instances_;
    if (result.ok()) result = upload.write_buffer(instance_buffer_.ref(), 0, std::as_bytes(std::span{draw_instances_}));
    if (result.ok()) result = upload.submit();
    if (result.ok()) {
      const std::array attributes{
          granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
          granit::vertex_attribute{.location = 1, .format = granit::vertex_format::float32x3, .offset = sizeof(float) * 3}};
      const std::array instance_attributes{
          granit::vertex_attribute{.location = 2, .format = granit::vertex_format::float32x4},
          granit::vertex_attribute{.location = 3, .format = granit::vertex_format::float32x4, .offset = sizeof(float) * 4}};
      const std::array bindings{
          granit::mesh_vertex_buffer{.buffer = vertex_buffer_.ref(), .layout = {.stride = sizeof(tutorial_transparency::vertex), .attributes = attributes}},
          granit::mesh_vertex_buffer{.buffer = instance_buffer_.ref(), .layout = {.stride = sizeof(instance_data), .step_mode = granit::vertex_step_mode::instance, .attributes = instance_attributes}}};
      result = mesh_.initialize(renderer_owner(), {.vertex_buffers = bindings, .index_buffer = index_buffer_.ref(),
        .index_format = granit::index_type::uint16, .index_count = static_cast<std::uint32_t>(tutorial_transparency::indices.size()), .instance_count = object_count});
    }
    return result;
  }

  granit::result initialize_layout() noexcept {
    const std::array entries{granit::bind_group_layout_entry{.binding = 0, .type = granit::binding_type::dynamic_uniform_buffer,
      .visibility = granit::shader_stage_flags::vertex | granit::shader_stage_flags::fragment}};
    auto result = layout_.initialize(renderer_owner(), entries);
    const std::array layouts{layout_.ref()};
    if (result.ok()) result = pipeline_layout_.initialize(renderer_owner(), layouts);
    const std::array resources{granit::bind_group_entry{.binding = 0, .resource = uniform_buffer_.ref(), .size = sizeof(scene_uniforms)}};
    if (result.ok()) result = group_.initialize(renderer_owner(), layout_, resources);
    return result;
  }

  granit::result create_depth() noexcept {
    auto result = depth_view_.reset();
    if (result.ok()) result = depth_.reset();
    if (result.ok()) result = depth_.initialize(renderer_owner(), {.format = granit::texture_format::d32_float,
      .usage = granit::texture_usage::depth_stencil_attachment, .width = presentation_info().width, .height = presentation_info().height});
    if (result.ok()) result = depth_view_.initialize(renderer_owner(), depth_);
    return result;
  }

  granit::result create_pipeline() noexcept {
    auto result = create_depth();
    if (result.failed()) return result;
    result = pipeline_.reset();
    if (result.ok()) result = blend_pipeline_.reset();
    const auto format = presentation_info().format;
    const std::array vertex_layouts{
      granit::vertex_buffer_layout{.stride = sizeof(tutorial_transparency::vertex), .attributes = std::array{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{.location = 1, .format = granit::vertex_format::float32x3, .offset = sizeof(float) * 3}}},
      granit::vertex_buffer_layout{.stride = sizeof(instance_data), .step_mode = granit::vertex_step_mode::instance,
        .attributes = std::array{granit::vertex_attribute{.location = 2, .format = granit::vertex_format::float32x4},
                                 granit::vertex_attribute{.location = 3, .format = granit::vertex_format::float32x4, .offset = sizeof(float) * 4}}}};
    const std::array blends{granit::color_blend_state{.source_color_factor = granit::blend_factor::source_alpha,
      .destination_color_factor = granit::blend_factor::one_minus_source_alpha, .source_alpha_factor = granit::blend_factor::one,
      .destination_alpha_factor = granit::blend_factor::one_minus_source_alpha}};
    if (result.ok()) result = pipeline_.initialize(renderer_owner(), {.layout = pipeline_layout_.ref(), .vertex_shader = vertex_shader_.ref(),
      .fragment_shader = fragment_shader_.ref(), .color_formats = std::span{&format, 1}, .depth_stencil_format = granit::texture_format::d32_float,
      .vertex_buffers = vertex_layouts, .primitive = {.cull = pipeline_cull_none_ ? granit::cull_mode::none : granit::cull_mode::back}, .depth = granit::depth_state{.test_enabled = true, .write_enabled = true},
      .color_blends = {}, .depth_bias = std::nullopt});
    if (result.ok()) result = blend_pipeline_.initialize(renderer_owner(), {.layout = pipeline_layout_.ref(), .vertex_shader = vertex_shader_.ref(),
      .fragment_shader = fragment_shader_.ref(), .color_formats = std::span{&format, 1}, .depth_stencil_format = granit::texture_format::d32_float,
      .vertex_buffers = vertex_layouts, .primitive = {.cull = pipeline_cull_none_ ? granit::cull_mode::none : granit::cull_mode::back}, .depth = granit::depth_state{.test_enabled = true, .write_enabled = false},
      .color_blends = blends, .depth_bias = std::nullopt});
    if (result.ok()) pipeline_format_ = format;
    return result;
  }

  granit::result on_swapchain_changed(const granit::swapchain_info& info) noexcept override {
    return info.format == pipeline_format_ ? create_depth() : create_pipeline();
  }

  void sort_instances(const granit::math::float3& camera_position) noexcept {
    std::array<std::size_t, object_count> solid{};
    std::array<std::size_t, object_count> blend{};
    std::size_t solid_count = 0;
    std::size_t blend_count = 0;
    for (std::size_t index = 0; index < object_count; ++index) {
      if (!opaque_only_debug_ && instances_[index].color_mode[3] == 2.0F)
        blend[blend_count++] = index;
      else
        solid[solid_count++] = index;
    }
    const auto distance_squared = [&](std::size_t index) {
      const auto& position = instances_[index].transform;
      const auto x = position[0] - camera_position.x;
      const auto y = position[1] - camera_position.y;
      const auto z = position[2] - camera_position.z;
      return x * x + y * y + z * z;
    };
    std::sort(blend.begin(), blend.begin() + static_cast<std::ptrdiff_t>(blend_count),
              [&](std::size_t left, std::size_t right) {
                return distance_squared(left) > distance_squared(right);
              });
    std::size_t output = 0;
    for (std::size_t index = 0; index < solid_count; ++index)
      draw_instances_[output++] = instances_[solid[index]];
    for (std::size_t index = 0; index < blend_count; ++index)
      draw_instances_[output++] = instances_[blend[index]];
    solid_instance_count_ = static_cast<std::uint32_t>(solid_count);
    blend_instance_count_ = static_cast<std::uint32_t>(blend_count);
  }

  granit::result update_light_gizmo() noexcept {
    auto result = debug_draw_.clear();
    if (result.failed() || !light_gizmo_)
      return result;
    constexpr std::array<float, 3> light_position{0.0F, 3.0F, 0.0F};
    const auto direction = normalized_or_up(light_direction_);
    const std::array<float, 3> light_travel_direction{-direction[0], -direction[1], -direction[2]};
    std::array<granit::debug_draw_line, 64> lines{};
    std::size_t line_count = 0;
    constexpr std::uint32_t light_color = 0xffffe040;
    append_gizmo_square(lines, line_count, light_position, 0.55F, light_color);
    append_gizmo_arrow(lines, line_count, light_position, light_travel_direction, 1.15F, light_color);
    return debug_draw_.append_lines(std::span{lines}.first(line_count));
  }

  void on_shutdown(granit::result) noexcept override {
    static_cast<void>(debug_draw_.destroy()); runtime_.shutdown(); static_cast<void>(pipeline_.reset()); static_cast<void>(blend_pipeline_.reset()); static_cast<void>(group_.reset()); static_cast<void>(pipeline_layout_.reset());
    static_cast<void>(layout_.reset()); static_cast<void>(uniform_buffer_.reset()); static_cast<void>(mesh_.reset());
    static_cast<void>(instance_buffer_.reset()); static_cast<void>(index_buffer_.reset()); static_cast<void>(vertex_buffer_.reset());
    static_cast<void>(depth_view_.reset()); static_cast<void>(depth_.reset()); static_cast<void>(fragment_shader_.reset());
    static_cast<void>(vertex_shader_.reset()); static_cast<void>(shaders_.reset()); static_cast<void>(frame_context_.reset());
  }
  granit::result on_window_event(const granit::window_event& event) noexcept override { runtime_.process(event); camera_input_.process(event); return granit::result::success; }
  granit::result on_input_event(const granit::input_event& event) noexcept override { runtime_.process(event); camera_input_.process(event, runtime_.wants_mouse(), runtime_.wants_keyboard()); return granit::result::success; }

  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    camera_input_.begin_frame(); granit::window_state state; auto result = app_window().get_state(state);
    if (result.ok()) result = runtime_.begin_frame(state, frame.delta_seconds, {.name = "10 Transparency Gallery", .description = "Opaque, mask, blend and object sorting", .frame = rendered_frames()});
    if (result.ok()) {
      ImGui::Checkbox("Auto orbit", &auto_orbit_); ImGui::Text("Opaque: 1  Mask: 2  Blend: 3");
      ImGui::SliderFloat3("Light direction", light_direction_.data(), -1.0F, 1.0F);
      ImGui::SliderFloat("Ambient", &ambient_strength_, 0.0F, 1.0F);
      ImGui::Checkbox("Light gizmo", &light_gizmo_);
      ImGui::Checkbox("Normal debug", &normal_debug_);
      ImGui::Checkbox("Diffuse debug", &diffuse_debug_);
      ImGui::Checkbox("Opaque only debug", &opaque_only_debug_);
      ImGui::Checkbox("Cull none debug", &cull_none_debug_);
      if (result.ok() && cull_none_debug_ != pipeline_cull_none_) {
        pipeline_cull_none_ = cull_none_debug_;
        result = create_pipeline();
      }
      ImGui::Text("OIT: unsupported; Blend uses object-center sorting");
      if (ImGui::Button("Reset camera")) camera_.reset(); result = runtime_.end_frame();
    }
    if (result.failed()) return result;
    const auto input = camera_input_.finish(runtime_.wants_mouse(), runtime_.wants_keyboard());
    if (auto_orbit_ && !smoke_test_) static_cast<void>(camera_.orbit(frame.delta_seconds * 0.25F));
    if (!camera_.update(input, frame.swapchain.width, frame.swapchain.height)) return granit::result::invalid_argument;
    granit::example::camera::camera_matrices matrices;
    if (!camera_.matrices(frame.swapchain.width, frame.swapchain.height, matrices)) return granit::result::invalid_argument;
    if (result.ok()) result = update_light_gizmo();
    sort_instances(matrices.position);
    const scene_uniforms uniforms{
        .view_projection = matrices.view_projection,
        .light_direction = {light_direction_[0], light_direction_[1], light_direction_[2], 0.0F},
        .ambient = {ambient_strength_, ambient_strength_, ambient_strength_, 0.0F},
        .debug = {normal_debug_ ? 1.0F : 0.0F, opaque_only_debug_ ? 1.0F : 0.0F,
                  diffuse_debug_ ? 1.0F : 0.0F, 0.0F}};
    granit::frame_recording recording; if (result.ok()) result = frame_context_.begin(frame.acquired, recording);
    const auto uniform_offset = uniform_stride_ * recording.frame_slot();
    if (result.ok()) result = instance_buffer_.write(0, std::as_bytes(std::span{draw_instances_}));
    if (result.ok()) result = uniform_buffer_.write(uniform_offset, std::as_bytes(std::span{&uniforms, 1}));
    const granit::viewport viewport{0, 0, static_cast<float>(frame.swapchain.width), static_cast<float>(frame.swapchain.height), 0, 1};
    const granit::scissor scissor{0, 0, frame.swapchain.width, frame.swapchain.height};
    const granit::color_attachment_desc color{.view = frame.backbuffer.view, .resolve_view = {}, .clear_value = {.red = 0.015F, .green = 0.02F, .blue = 0.04F, .alpha = 1.0F}};
    const granit::depth_stencil_attachment_desc depth{.view = depth_view_.ref(), .clear_value = {.depth = 1.0F}};
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1}, .depth_stencil_attachment = &depth, .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
    auto& recorder = recording.recorder();
    if (result.ok()) result = recorder.set_viewports(0, std::span{&viewport, 1});
    if (result.ok()) result = recorder.set_scissors(0, std::span{&scissor, 1});
    if (result.ok()) result = recorder.bind_graphics_pipeline(pipeline_);
    if (result.ok()) result = mesh_.bind(recorder);
    const std::array offsets{static_cast<std::uint32_t>(uniform_offset)};
    if (result.ok()) result = recorder.bind_graphics_group(pipeline_layout_, 0, group_, offsets);
    if (result.ok()) result = recorder.begin_rendering(rendering);
    if (result.ok()) result = mesh_.draw(recorder, {.instance_count = solid_instance_count_});
    if (result.ok() && blend_instance_count_ != 0)
      result = recorder.bind_graphics_pipeline(blend_pipeline_);
    if (result.ok() && blend_instance_count_ != 0)
      result = mesh_.draw(recorder, {.instance_count = blend_instance_count_,
                                     .first_instance = solid_instance_count_});
    if (result.ok()) result = recorder.end_rendering();
    if (result.ok() && light_gizmo_) {
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
    if (result.ok()) result = runtime_.canvas().record(recorder, {.color = frame.backbuffer.view, .color_format = frame.swapchain.format,
      .width = frame.swapchain.width, .height = frame.swapchain.height, .load_operation = granit::attachment_load_operation::load,
      .encode_srgb = true, .frame_slot = recording.frame_slot()});
    if (result.ok()) result = recording.submit();
    if (result.failed() && recording.valid()) static_cast<void>(recording.abort());
    return result;
  }

  granit::example::tutorial::tutorial_runtime runtime_; granit::example::camera::orbit_camera camera_;
  granit::example::camera::orbit_camera_input_accumulator camera_input_; granit::frame_context frame_context_;
  granit::debug_draw_list debug_draw_;
  granit::shader_library shaders_; granit::shader vertex_shader_, fragment_shader_; granit::buffer vertex_buffer_, index_buffer_, instance_buffer_, uniform_buffer_;
  granit::mesh mesh_; granit::texture depth_; granit::texture_view depth_view_; granit::bind_group_layout layout_; granit::pipeline_layout pipeline_layout_; granit::bind_group group_; granit::graphics_pipeline pipeline_, blend_pipeline_;
  std::array<instance_data, object_count> instances_{}; std::array<instance_data, object_count> draw_instances_{}; granit::texture_format pipeline_format_{granit::texture_format::undefined};
  std::array<float, 3> light_direction_{0.35F, 0.8F, 0.45F}; float ambient_strength_{0.12F};
  std::uint64_t uniform_stride_{}; std::uint32_t solid_instance_count_{}; std::uint32_t blend_instance_count_{}; bool smoke_test_{}; bool auto_orbit_{}; bool light_gizmo_{true}; bool normal_debug_{}; bool diffuse_debug_{}; bool opaque_only_debug_{}; bool cull_none_debug_{}; bool pipeline_cull_none_{};
};
application app;
} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_10_rendered_frames() noexcept { return app.rendered_frames(); }
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_10_recreate_count() noexcept { return app.completed_recreates(); }
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_10_feature_value() noexcept { return object_count; }
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_10_canvas_items() noexcept { return app.canvas_items(); }
extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_10_ready() noexcept { return app.ready() ? 1 : 0; }
#endif

int main(int argc, char** argv) {
  const bool smoke = argc == 2 && std::string_view{argv[1]} == "--smoke-test"; app.set_smoke_test(smoke);
  const auto result = app.run({.executable_path = argc > 0 ? argv[0] : "", .title = "Granit Transparency Gallery",
    .renderer = {.application_name = "Granit Transparency Gallery", .presentation = granit::presentation_mode::enabled}, .swapchain = {}, .smoke_test = smoke});
  if (smoke && result == granit::result::backend_unavailable) return 77;
  return result.failed() ? failure("application run", result) : 0;
}
