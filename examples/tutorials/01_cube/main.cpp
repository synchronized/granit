// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "assets/asset_manager.h"
#include "camera/orbit_camera.h"
#include "camera/orbit_camera_input_accumulator.h"
#include "gltf/scene.h"
#include "shader_archive.h"
#include "tutorial/tutorial_runtime.h"

#include <granit/math/functions.hpp>
#include <granit/pipeline/mesh.hpp>
#include <imgui.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include "model_data.hpp"

namespace {

using granit::math::matrix4;

bool make_model_view_projection(float angle, const granit::example::camera::camera_matrices& camera,
                                matrix4& output) {
  const auto model = granit::math::rotation_y_matrix4(angle);
  output = granit::math::multiply(camera.view_projection, model);
  return true;
}

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
    if (!runtime_.initialized()) {
      auto result = runtime_.initialize(renderer_owner());
      if (result.failed())
        return result;
      crate_image_ = assets().load<granit::example::gltf::image>(
          granit::example::assets::asset_location::bundled("tutorials/01_cube/wooden_crate.png"));
    }
    if (!crate_image_.ready()) {
      if (crate_image_.status() == granit::example::assets::asset_status::failed ||
          crate_image_.status() == granit::example::assets::asset_status::cancelled) {
        std::cerr << "Failed to load crate texture: " << crate_image_.diagnostic() << '\n';
        return granit::result::invalid_argument;
      }
      return granit::result::not_ready;
    }

    auto result = create_depth_target();
    if (result.ok())
      result = frame_context_.initialize(renderer());
    if (result.ok())
      result = shader_library_.initialize(renderer(), tutorial_cube::shader_archive());
    if (result.ok())
      result = shader_library_.create_shader("mesh.vertex", vertex_shader_);
    if (result.ok())
      result = shader_library_.create_shader("mesh.fragment", fragment_shader_);
    if (result.ok())
      result = initialize_texture_resources(*crate_image_.value());
    if (result.ok())
      result = initialize_pipeline_layout();
    if (result.ok())
      result = create_pipeline();
    if (result.ok()) {
      result =
          runtime_.register_texture(crate_view_.ref(), crate_sampler_.ref(), crate_texture_id_);
    }
    if (result.ok() &&
        !camera_.focus({.radius = 1.5F}, presentation_info().width, presentation_info().height))
      result = granit::result::invalid_argument;
    return result;
  }

  granit::result on_swapchain_changed(const granit::swapchain_info& info) noexcept override {
    auto result = create_depth_target();
    if (result.ok() && info.format != pipeline_format_)
      result = create_pipeline();
    return result;
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

  void on_shutdown(granit::result) noexcept override {
    runtime_.shutdown();
    static_cast<void>(pipeline_.reset());
    static_cast<void>(resource_group_.reset());
    static_cast<void>(layout_.reset());
    static_cast<void>(resource_layout_.reset());
    static_cast<void>(uniform_buffer_.reset());
    static_cast<void>(mesh_.reset());
    static_cast<void>(index_buffer_.reset());
    static_cast<void>(vertex_buffer_.reset());
    static_cast<void>(crate_sampler_.reset());
    static_cast<void>(crate_view_.reset());
    static_cast<void>(crate_texture_.reset());
    static_cast<void>(depth_view_.reset());
    static_cast<void>(depth_texture_.reset());
    static_cast<void>(fragment_shader_.reset());
    static_cast<void>(vertex_shader_.reset());
    static_cast<void>(shader_library_.reset());
    static_cast<void>(frame_context_.reset());
  }

  granit::result
  initialize_texture_resources(const granit::example::gltf::image& decoded_texture) noexcept {
    if (decoded_texture.mips.size() != 1) {
      std::cerr << "Crate texture has an unsupported mip layout\n";
      return granit::result::invalid_argument;
    }
    const auto& mip = decoded_texture.mips.front();
    auto result = crate_texture_.initialize(
        renderer_owner(),
        {.format = granit::texture_format::rgba8_srgb,
         .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
         .width = mip.width,
         .height = mip.height});
    if (result.ok())
      result = crate_view_.initialize(renderer_owner(), crate_texture_);
    if (result.ok()) {
      result = crate_sampler_.initialize(renderer_owner(),
                                         {.mag_filter = granit::filter::linear,
                                          .min_filter = granit::filter::linear,
                                          .mip_filter = granit::mipmap_filter::nearest});
    }
    if (result.ok()) {
      result = vertex_buffer_.initialize(
          renderer_owner(),
          {.size = sizeof(tutorial_model::vertices),
           .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination});
    }
    if (result.ok()) {
      result = index_buffer_.initialize(
          renderer_owner(),
          {.size = sizeof(tutorial_model::indices),
           .usage = granit::buffer_usage::index | granit::buffer_usage::transfer_destination});
    }
    granit::renderer_limits limits;
    if (result.ok())
      result = renderer_owner().get_limits(limits);
    if (result.ok()) {
      uniform_stride_ = align_up(sizeof(matrix4), limits.uniform_buffer_offset_alignment);
      result = uniform_buffer_.initialize(
          renderer_owner(),
          {.size = uniform_stride_ * GRANIT_DEFAULT_FRAMES_IN_FLIGHT,
           .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination});
    }

    granit::upload_batch upload;
    if (result.ok())
      result = upload.initialize(renderer_owner());
    if (result.ok()) {
      result = upload.write_texture(crate_texture_.ref(), decoded_texture.rgba8_pixels,
                                    {.bytes_per_row = mip.width * 4, .rows_per_image = mip.height},
                                    {.width = mip.width, .height = mip.height});
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
        renderer_owner(),
        {.topology = granit::primitive_topology::triangle_list,
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
    auto result = resource_layout_.initialize(renderer_owner(), entries);
    const std::array layout_refs{resource_layout_.ref()};
    if (result.ok())
      result = layout_.initialize(renderer_owner(), layout_refs);

    const std::array resources{
        granit::bind_group_entry{
            .binding = 0, .resource = uniform_buffer_.ref(), .size = sizeof(matrix4)},
        granit::bind_group_entry{.binding = 1, .resource = crate_view_.ref()},
        granit::bind_group_entry{.binding = 2, .resource = crate_sampler_.ref()},
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
    const std::array attributes{
        granit::vertex_attribute{.location = 0, .format = granit::vertex_format::float32x3},
        granit::vertex_attribute{
            .location = 1, .format = granit::vertex_format::float32x2, .offset = sizeof(float) * 3},
    };
    const std::array vertex_layouts{granit::vertex_buffer_layout{
        .stride = sizeof(tutorial_model::vertex), .attributes = attributes}};
    result = pipeline_.initialize(
        renderer_owner(),
        {.layout = layout_.ref(),
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
                                    {.name = "01 Cube",
                                     .description = "Low-level Renderer + Mesh + Canvas",
                                     .frame = rendered_frames()});
    }
    if (result.ok()) {
      ImGui::Checkbox("Rotate", &rotate_);
      ImGui::SliderFloat("Rotation speed", &rotation_speed_, 0.0F, 3.0F);
      ImGui::Checkbox("Auto orbit", &auto_orbit_);
      if (ImGui::Button("Reset camera"))
        camera_.reset();
      ImGui::Image(ImTextureRef{crate_texture_id_}, {64, 64});
      result = runtime_.end_frame();
    }

    granit::frame_recording recording;
    if (result.ok())
      result = frame_context_.begin(frame.acquired, recording);

    if (rotate_)
      rotation_ += frame.delta_seconds * rotation_speed_;
    const auto input = camera_input_.finish(runtime_.wants_mouse(), runtime_.wants_keyboard());
    if (auto_orbit_ && !smoke_test_ && !camera_.orbit(frame.delta_seconds * 0.25F))
      result = granit::result::invalid_argument;
    if (result.ok() && !camera_.update(input, frame.swapchain.width, frame.swapchain.height))
      result = granit::result::invalid_argument;
    granit::example::camera::camera_matrices camera_matrices;
    if (result.ok() &&
        !camera_.matrices(frame.swapchain.width, frame.swapchain.height, camera_matrices))
      result = granit::result::invalid_argument;
    matrix4 matrix{};
    if (result.ok() && !make_model_view_projection(rotation_, camera_matrices, matrix))
      result = granit::result::invalid_argument;
    const auto uniform_offset = uniform_stride_ * recording.frame_slot();
    if (result.ok())
      result = uniform_buffer_.write(uniform_offset, std::as_bytes(std::span{&matrix, 1}));

    const granit::viewport viewport{
        0, 0, static_cast<float>(frame.swapchain.width), static_cast<float>(frame.swapchain.height),
        0, 1};
    const granit::scissor scissor{0, 0, frame.swapchain.width, frame.swapchain.height};
    const granit::color_attachment_desc color{
        .view = frame.backbuffer.view,
        .resolve_view = {},
        .clear_value = {.red = 0.04F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
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
      result = recorder.bind_graphics_group(layout_, 0, resource_group_, dynamic_offsets);
    if (result.ok())
      result = recorder.begin_rendering(rendering);
    if (result.ok())
      result = mesh_.draw(recorder);
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
  granit::example::assets::asset_handle<granit::example::gltf::image> crate_image_;
  granit::shader_library shader_library_;
  granit::shader vertex_shader_;
  granit::shader fragment_shader_;
  granit::texture crate_texture_;
  granit::texture_view crate_view_;
  granit::sampler crate_sampler_;
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
  granit::example::tutorial::tutorial_runtime runtime_;
  granit::example::camera::orbit_camera camera_;
  granit::example::camera::orbit_camera_input_accumulator camera_input_;
  ImTextureID crate_texture_id_{ImTextureID_Invalid};
  granit::texture_format pipeline_format_{granit::texture_format::undefined};
  std::uint64_t uniform_stride_{};
  float rotation_{};
  float rotation_speed_{1.0F};
  bool rotate_{true};
  bool auto_orbit_{};
  bool smoke_test_{};
};

tutorial_application application;

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_01_rendered_frames() noexcept {
  return application.rendered_frames();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_01_recreate_count() noexcept {
  return application.completed_recreates();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_01_canvas_items() noexcept {
  return application.canvas_items();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_01_ready() noexcept {
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
                       .title = "Granit Cube",
                       .renderer = {.application_name = "Granit Cube",
                                    .presentation = granit::presentation_mode::enabled},
                       .swapchain = {},
                       .smoke_test = smoke_test});
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  if (result.failed())
    return report_failure("application run", result);
  return 0;
}
