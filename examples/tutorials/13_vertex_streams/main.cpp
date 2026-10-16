// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "camera/orbit_camera.h"
#include "camera/orbit_camera_input_accumulator.h"
#include "tutorial/tutorial_runtime.h"
#include "shader_archive.h"

#include "../../common/geometry/primitives.hpp"

#include <granit/pipeline/mesh.hpp>
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

using granit::math::matrix4;

struct scene_uniforms {
  matrix4 view_projection;
  std::array<float, 4> light_direction{};
};

struct base_vertex {
  std::array<float, 3> position{};
  std::array<float, 3> normal{};
};

struct surface_vertex {
  std::array<float, 2> uv{};
  std::array<float, 4> color{};
};

struct interleaved_vertex {
  base_vertex base{};
  surface_vertex surface{};
};

static_assert(sizeof(scene_uniforms) == sizeof(float) * 20);
static_assert(sizeof(base_vertex) == sizeof(float) * 6);
static_assert(sizeof(surface_vertex) == sizeof(float) * 6);
static_assert(sizeof(interleaved_vertex) == sizeof(float) * 12);

constexpr std::array<std::array<float, 2>, 4> cube_uv{{
    {{0.0F, 1.0F}}, {{1.0F, 1.0F}}, {{1.0F, 0.0F}}, {{0.0F, 0.0F}},
}};

constexpr std::array<std::array<float, 4>, 6> face_colors{{
    {{0.92F, 0.25F, 0.18F, 1.0F}}, {{0.18F, 0.72F, 0.94F, 1.0F}},
    {{0.32F, 0.88F, 0.36F, 1.0F}}, {{0.92F, 0.72F, 0.18F, 1.0F}},
    {{0.78F, 0.30F, 0.90F, 1.0F}}, {{0.96F, 0.48F, 0.18F, 1.0F}},
}};

void make_geometry(std::array<base_vertex, 24>& base, std::array<surface_vertex, 24>& surface,
                   std::array<interleaved_vertex, 24>& interleaved) {
  for (std::size_t index = 0; index < base.size(); ++index) {
    const auto& source = granit::example::geometry::cube_vertices[index];
    const auto face = index / 4;
    const auto corner = index % 4;
    base[index] = {.position = {source.position[0], source.position[1], source.position[2]},
                   .normal = {source.normal[0], source.normal[1], source.normal[2]}};
    surface[index] = {.uv = cube_uv[corner], .color = face_colors[face]};
    interleaved[index] = {.base = base[index], .surface = surface[index]};
  }
}

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

std::uint64_t align_up(std::uint64_t value, std::uint64_t alignment) {
  return alignment == 0 ? value : (value + alignment - 1) / alignment * alignment;
}

class tutorial_application final : public granit::example::application {
public:
  void set_smoke_test(bool enabled) noexcept { smoke_test_ = enabled; }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return runtime_.canvas_items(); }

private:
  granit::result on_initialize() noexcept override {
    auto result = runtime_.initialize(renderer_owner());
    if (result.ok())
      make_geometry(base_vertices_, surface_vertices_, interleaved_vertices_);
    if (result.ok())
      result = frame_context_.initialize(renderer());
    if (result.ok())
      result = shader_library_.initialize(renderer(), tutorial_vertex_streams::shader_archive());
    if (result.ok())
      result = shader_library_.create_shader("vertex_streams.vertex", vertex_shader_);
    if (result.ok())
      result = shader_library_.create_shader("vertex_streams.fragment", fragment_shader_);
    if (result.ok())
      result = initialize_buffers();
    if (result.ok())
      result = initialize_bindings();
    if (result.ok())
      result = create_depth_target();
    if (result.ok())
      result = create_pipelines();
    if (result.ok() && !camera_.focus({.radius = 2.8F}, presentation_info().width,
                                      presentation_info().height))
      result = granit::result::invalid_argument;
    return result;
  }

  granit::result on_swapchain_changed(const granit::swapchain_info& info) noexcept override {
    auto result = create_depth_target();
    if (result.ok() && info.format != pipeline_format_)
      result = create_pipelines();
    return result;
  }

  void on_shutdown(granit::result) noexcept override {
    runtime_.shutdown();
    static_cast<void>(split_pipeline_.reset());
    static_cast<void>(interleaved_pipeline_.reset());
    static_cast<void>(resource_group_.reset());
    static_cast<void>(pipeline_layout_.reset());
    static_cast<void>(resource_layout_.reset());
    static_cast<void>(uniform_buffer_.reset());
    static_cast<void>(split_mesh_.reset());
    static_cast<void>(interleaved_mesh_.reset());
    static_cast<void>(base_buffer_.reset());
    static_cast<void>(surface_buffer_.reset());
    static_cast<void>(interleaved_buffer_.reset());
    static_cast<void>(index_buffer_.reset());
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

  granit::result initialize_buffers() noexcept {
    auto result = base_buffer_.initialize(
        renderer_owner(), {.size = sizeof(base_vertices_),
                           .usage = granit::buffer_usage::vertex |
                                    granit::buffer_usage::transfer_destination});
    if (result.ok())
      result = surface_buffer_.initialize(
          renderer_owner(), {.size = sizeof(surface_vertices_),
                             .usage = granit::buffer_usage::vertex |
                                      granit::buffer_usage::transfer_destination});
    if (result.ok())
      result = interleaved_buffer_.initialize(
          renderer_owner(), {.size = sizeof(interleaved_vertices_),
                             .usage = granit::buffer_usage::vertex |
                                      granit::buffer_usage::transfer_destination});
    if (result.ok())
      result = index_buffer_.initialize(
          renderer_owner(), {.size = sizeof(granit::example::geometry::cube_indices),
                             .usage = granit::buffer_usage::index |
                                      granit::buffer_usage::transfer_destination});
    granit::renderer_limits limits;
    if (result.ok())
      result = renderer_owner().get_limits(limits);
    if (result.ok()) {
      uniform_stride_ = align_up(sizeof(scene_uniforms), limits.uniform_buffer_offset_alignment);
      result = uniform_buffer_.initialize(
          renderer_owner(), {.size = uniform_stride_ * GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
                             .usage = granit::buffer_usage::uniform |
                                      granit::buffer_usage::transfer_destination});
    }
    granit::upload_batch upload;
    if (result.ok())
      result = upload.initialize(renderer_owner());
    if (result.ok())
      result = upload.write_buffer(base_buffer_.ref(), 0, std::as_bytes(std::span{base_vertices_}));
    if (result.ok())
      result = upload.write_buffer(surface_buffer_.ref(), 0,
                                   std::as_bytes(std::span{surface_vertices_}));
    if (result.ok())
      result = upload.write_buffer(interleaved_buffer_.ref(), 0,
                                   std::as_bytes(std::span{interleaved_vertices_}));
    if (result.ok())
      result = upload.write_buffer(index_buffer_.ref(), 0,
                                   std::as_bytes(std::span{granit::example::geometry::cube_indices}));
    if (result.ok())
      result = upload.submit();
    if (result.ok()) {
      const std::array attributes{
          granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
          granit::vertex_attribute{.location = 1, .format = granit::vertex_format::float32x3,
                                   .offset = sizeof(float) * 3},
      };
      const std::array surface_attributes{
          granit::vertex_attribute{.location = 2, .format = granit::vertex_format::float32x2},
          granit::vertex_attribute{.location = 3, .format = granit::vertex_format::float32x4,
                                   .offset = sizeof(float) * 2},
      };
      const std::array split_bindings{
          granit::mesh_vertex_buffer{.buffer = base_buffer_.ref(),
                                     .layout = {.stride = sizeof(base_vertex),
                                                .attributes = attributes}},
          granit::mesh_vertex_buffer{.buffer = surface_buffer_.ref(),
                                     .layout = {.stride = sizeof(surface_vertex),
                                                .attributes = surface_attributes}},
      };
      result = split_mesh_.initialize(
          renderer_owner(), {.vertex_buffers = split_bindings,
                             .index_buffer = index_buffer_.ref(),
                             .index_format = granit::index_type::uint16,
                             .index_count = static_cast<std::uint32_t>(
                                 granit::example::geometry::cube_indices.size())});
      if (result.ok()) {
        const std::array interleaved_attributes{
            granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
            granit::vertex_attribute{.location = 1, .format = granit::vertex_format::float32x3,
                                     .offset = sizeof(float) * 3},
            granit::vertex_attribute{.location = 2, .format = granit::vertex_format::float32x2,
                                     .offset = sizeof(float) * 6},
            granit::vertex_attribute{.location = 3, .format = granit::vertex_format::float32x4,
                                     .offset = sizeof(float) * 8},
        };
        const std::array interleaved_bindings{granit::mesh_vertex_buffer{
            .buffer = interleaved_buffer_.ref(),
            .layout = {.stride = sizeof(interleaved_vertex),
                       .attributes = interleaved_attributes}}};
        result = interleaved_mesh_.initialize(
            renderer_owner(), {.vertex_buffers = interleaved_bindings,
                               .index_buffer = index_buffer_.ref(),
                               .index_format = granit::index_type::uint16,
                               .index_count = static_cast<std::uint32_t>(
                                   granit::example::geometry::cube_indices.size())});
      }
    }
    return result;
  }

  granit::result initialize_bindings() noexcept {
    const std::array entries{granit::bind_group_layout_entry{
        .binding = 0,
        .type = granit::binding_type::dynamic_uniform_buffer,
        .visibility = granit::shader_stage_flags::vertex | granit::shader_stage_flags::fragment}};
    auto result = resource_layout_.initialize(renderer_owner(), entries);
    const std::array layouts{resource_layout_.ref()};
    if (result.ok())
      result = pipeline_layout_.initialize(renderer_owner(), layouts);
    const std::array resources{granit::bind_group_entry{
        .binding = 0, .resource = uniform_buffer_.ref(), .size = sizeof(scene_uniforms)}};
    if (result.ok())
      result = resource_group_.initialize(renderer_owner(), resource_layout_, resources);
    return result;
  }

  granit::result create_depth_target() noexcept {
    auto result = depth_view_.reset();
    if (result.ok())
      result = depth_texture_.reset();
    if (result.ok())
      result = depth_texture_.initialize(renderer_owner(),
                                         {.format = granit::texture_format::d32_float,
                                          .usage = granit::texture_usage::depth_stencil_attachment,
                                          .width = presentation_info().width,
                                          .height = presentation_info().height});
    if (result.ok())
      result = depth_view_.initialize(renderer_owner(), depth_texture_);
    return result;
  }

  granit::result create_pipelines() noexcept {
    auto result = split_pipeline_.reset();
    if (result.ok())
      result = interleaved_pipeline_.reset();
    if (result.failed())
      return result;
    const auto format = presentation_info().format;
    const std::array base_attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{.location = 1, .format = granit::vertex_format::float32x3,
                                 .offset = sizeof(float) * 3},
    };
    const std::array surface_attributes{
        granit::vertex_attribute{.location = 2, .format = granit::vertex_format::float32x2},
        granit::vertex_attribute{.location = 3, .format = granit::vertex_format::float32x4,
                                 .offset = sizeof(float) * 2},
    };
    const std::array split_layouts{
        granit::vertex_buffer_layout{.stride = sizeof(base_vertex), .attributes = base_attributes},
        granit::vertex_buffer_layout{.stride = sizeof(surface_vertex),
                                     .attributes = surface_attributes},
    };
    const std::array interleaved_attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{.location = 1, .format = granit::vertex_format::float32x3,
                                 .offset = sizeof(float) * 3},
        granit::vertex_attribute{.location = 2, .format = granit::vertex_format::float32x2,
                                 .offset = sizeof(float) * 6},
        granit::vertex_attribute{.location = 3, .format = granit::vertex_format::float32x4,
                                 .offset = sizeof(float) * 8},
    };
    const std::array interleaved_layouts{granit::vertex_buffer_layout{
        .stride = sizeof(interleaved_vertex), .attributes = interleaved_attributes}};
    const auto make_desc = [&](std::span<const granit::vertex_buffer_layout> layouts) {
      return granit::graphics_pipeline_desc{
          .layout = pipeline_layout_.ref(),
          .vertex_shader = vertex_shader_.ref(),
          .fragment_shader = fragment_shader_.ref(),
          .color_formats = std::span{&format, 1},
          .depth_stencil_format = granit::texture_format::d32_float,
          .vertex_buffers = layouts,
          .primitive = {.cull = granit::cull_mode::back},
          .depth = granit::depth_state{.test_enabled = true, .write_enabled = true},
          .color_blends = {},
          .depth_bias = std::nullopt};
    };
    result = split_pipeline_.initialize(renderer_owner(), make_desc(split_layouts));
    if (result.ok())
      result = interleaved_pipeline_.initialize(renderer_owner(), make_desc(interleaved_layouts));
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
                                    {.name = "13 Vertex Streams",
                                     .description = "Split and interleaved vertex buffer layouts",
                                     .frame = rendered_frames()});
    if (result.ok()) {
      ImGui::Checkbox("Interleaved Vertex Buffer", &interleaved_);
      ImGui::Checkbox("Auto orbit", &auto_orbit_);
      if (ImGui::Button("Reset camera"))
        camera_.reset();
      ImGui::Text("Layout: %s", interleaved_ ? "1 buffer / 4 attributes" :
                                            "2 buffers / 2+2 attributes");
      ImGui::Text("Index Buffer: 36 uint16 indices");
      ImGui::TextUnformatted("Vertex buffers are bound before begin_rendering.");
      result = runtime_.end_frame();
    }
    if (result.failed())
      return result;

    granit::frame_recording recording;
    result = frame_context_.begin(frame.acquired, recording);
    const auto input = camera_input_.finish(runtime_.wants_mouse(), runtime_.wants_keyboard());
    if (auto_orbit_ && !smoke_test_ && !camera_.orbit(frame.delta_seconds * 0.25F))
      result = granit::result::invalid_argument;
    if (result.ok() && !camera_.update(input, frame.swapchain.width, frame.swapchain.height))
      result = granit::result::invalid_argument;
    granit::example::camera::camera_matrices matrices;
    if (result.ok() && !camera_.matrices(frame.swapchain.width, frame.swapchain.height, matrices))
      result = granit::result::invalid_argument;
    const scene_uniforms uniforms{.view_projection = matrices.view_projection,
                                  .light_direction = {0.45F, 0.8F, 0.35F, 0.0F}};
    const auto uniform_offset = uniform_stride_ * recording.frame_slot();
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
    const auto& pipeline = interleaved_ ? interleaved_pipeline_ : split_pipeline_;
    const auto& mesh = interleaved_ ? interleaved_mesh_ : split_mesh_;
    if (result.ok())
      result = recorder.set_viewports(0, std::span{&viewport, 1});
    if (result.ok())
      result = recorder.set_scissors(0, std::span{&scissor, 1});
    if (result.ok())
      result = recorder.bind_graphics_pipeline(pipeline);
    if (result.ok())
      result = mesh.bind(recorder);
    const std::array dynamic_offsets{static_cast<std::uint32_t>(uniform_offset)};
    if (result.ok())
      result = recorder.bind_graphics_group(pipeline_layout_, 0, resource_group_, dynamic_offsets);
    if (result.ok())
      result = recorder.begin_rendering(rendering);
    if (result.ok())
      result = mesh.draw(recorder);
    if (result.ok())
      result = recorder.end_rendering();
    if (result.ok()) {
      const bool encode_srgb = frame.swapchain.format == granit::texture_format::rgba8_unorm ||
                               frame.swapchain.format == granit::texture_format::bgra8_unorm;
      result = runtime_.canvas().record(
          recorder, {.color = frame.backbuffer.view,
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

  std::array<base_vertex, 24> base_vertices_{};
  std::array<surface_vertex, 24> surface_vertices_{};
  std::array<interleaved_vertex, 24> interleaved_vertices_{};
  granit::example::tutorial::tutorial_runtime runtime_;
  granit::example::camera::orbit_camera camera_;
  granit::example::camera::orbit_camera_input_accumulator camera_input_;
  granit::frame_context frame_context_;
  granit::shader_library shader_library_;
  granit::shader vertex_shader_;
  granit::shader fragment_shader_;
  granit::buffer base_buffer_;
  granit::buffer surface_buffer_;
  granit::buffer interleaved_buffer_;
  granit::buffer index_buffer_;
  granit::buffer uniform_buffer_;
  granit::mesh split_mesh_;
  granit::mesh interleaved_mesh_;
  granit::texture depth_texture_;
  granit::texture_view depth_view_;
  granit::bind_group_layout resource_layout_;
  granit::pipeline_layout pipeline_layout_;
  granit::bind_group resource_group_;
  granit::graphics_pipeline split_pipeline_;
  granit::graphics_pipeline interleaved_pipeline_;
  granit::texture_format pipeline_format_{granit::texture_format::undefined};
  std::uint64_t uniform_stride_{};
  bool interleaved_{};
  bool auto_orbit_{};
  bool smoke_test_{};
};

tutorial_application application;

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_13_canvas_items() noexcept {
  return application.canvas_items();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_13_ready() noexcept {
  return application.ready() ? 1 : 0;
}
#endif

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";
  const std::string_view executable_path =
      argument_count > 0 && arguments[0] != nullptr ? arguments[0] : "";
  application.set_smoke_test(smoke_test);
  const auto result = application.run({.executable_path = executable_path,
                                       .title = "Granit Vertex Streams",
                                       .renderer = {.application_name = "Granit Vertex Streams",
                                                    .presentation = granit::presentation_mode::enabled},
                                       .swapchain = {},
                                       .smoke_test = smoke_test});
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  if (result.failed())
    return report_failure("application run", result);
  return 0;
}
