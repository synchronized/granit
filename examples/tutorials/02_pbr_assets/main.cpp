// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"

#include <granit/integrations/imgui/renderer.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/material.hpp>
#include <granit/pipeline/mesh.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <imgui.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include "imgui_input.hpp"
#include "imgui_resources.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif
#include "model_data.hpp"

#ifndef GRANIT_TUTORIAL_02_SHADER_LIBRARY
#error "GRANIT_TUTORIAL_02_SHADER_LIBRARY must point to the generated Shader Library"
#endif
#ifndef GRANIT_TUTORIAL_02_MATERIAL
#error "GRANIT_TUTORIAL_02_MATERIAL must point to the generated Material archive"
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

matrix4 scale_and_translate(float x, float y, float z, float translation_y) {
  return {{x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, translation_y, 0, 1}};
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
  const auto byte_count = static_cast<std::uint64_t>(size);
  if (byte_count > std::numeric_limits<std::size_t>::max() ||
      byte_count > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
    return {};
  }
  stream.seekg(0, std::ios::beg);
  std::vector<std::byte> bytes(static_cast<std::size_t>(byte_count));
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(byte_count));
  return stream ? bytes : std::vector<std::byte>{};
}

class tutorial_application final : public granit::example::application {
public:
  [[nodiscard]] std::uint32_t pointer_events() const noexcept { return pointer_events_; }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return canvas_items_; }
  [[nodiscard]] granit_result shutdown_reason() const noexcept { return shutdown_reason_.native(); }

private:
  granit::result on_initialize() noexcept override {
    shader_archive_ = read_file(GRANIT_TUTORIAL_02_SHADER_LIBRARY);
    material_archive_ = read_file(GRANIT_TUTORIAL_02_MATERIAL);
    if (shader_archive_.empty() || material_archive_.empty()) {
      std::cerr << "Failed to read generated Shader Library or Material archive\n";
      return granit::result::invalid_argument;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    imgui_initialized_ = true;

    last_operation_ = "loading shader library";
    auto result = shader_library_.initialize(renderer(), shader_archive_);
    if (result.ok()) {
      last_operation_ = "creating geometry";
      result = initialize_geometry();
    }
    if (result.ok()) {
      last_operation_ = "creating materials";
      result = initialize_materials();
    }
    if (result.ok()) {
      last_operation_ = "creating render pipeline";
      result = pipeline_.initialize(renderer(), {.enable_fxaa = true, .enable_specular_aa = true});
    }
    if (result.ok()) {
      last_operation_ = "creating canvas";
      result = canvas_.initialize(renderer());
    }
    if (result.ok()) {
      last_operation_ = "uploading ImGui font";
      result = tutorial_imgui::upload_font_atlas(renderer_owner(), font_texture_, font_view_,
                                                 font_sampler_);
    }
    if (result.ok()) {
      last_operation_ = "uploading checker texture";
      result = tutorial_imgui::upload_checker(renderer_owner(), checker_texture_, checker_view_);
    }
    if (result.ok()) {
      imgui_bindings_ = {.font = {font_view_.ref(), font_sampler_.ref()},
                         .checker = {checker_view_.ref(), font_sampler_.ref()}};
    }
    return result;
  }

  granit::result on_window_event(const granit::window_event& event) noexcept override {
    tutorial_imgui::process_window_event(event);
    return granit::result::success;
  }

  granit::result on_input_event(const granit::input_event& event) noexcept override {
    tutorial_imgui::process_input_event(event);
    if (event.type == granit::input_event_type::pointer_moved ||
        event.type == granit::input_event_type::pointer_button ||
        event.type == granit::input_event_type::pointer_wheel) {
      ++pointer_events_;
    }
    return granit::result::success;
  }

  void on_shutdown(granit::result reason) noexcept override {
    shutdown_reason_ = reason;
    if (reason.failed())
      std::cerr << "window loop stopped during " << last_operation_ << ": " << reason.message()
                << '\n';
    static_cast<void>(scene_.reset());
    static_cast<void>(pipeline_.reset());
    static_cast<void>(canvas_.destroy());
    static_cast<void>(floor_material_.reset());
    static_cast<void>(cube_material_.reset());
    static_cast<void>(mesh_.reset());
    static_cast<void>(index_buffer_.reset());
    static_cast<void>(vertex_buffer_.reset());
    static_cast<void>(material_sampler_.reset());
    static_cast<void>(checker_view_.reset());
    static_cast<void>(checker_texture_.reset());
    static_cast<void>(font_view_.reset());
    static_cast<void>(font_texture_.reset());
    static_cast<void>(font_sampler_.reset());
    for (auto& view : material_views_)
      static_cast<void>(view.reset());
    for (auto& texture : material_textures_)
      static_cast<void>(texture.reset());
    static_cast<void>(shader_library_.reset());
    if (imgui_initialized_) {
      ImGui::DestroyContext();
      imgui_initialized_ = false;
    }
  }

  granit::result initialize_geometry() noexcept {
    model_ = tutorial_model::make_uv_sphere();
    auto result = vertex_buffer_.initialize(
        renderer_owner(),
        {.size = model_.vertices.size() * sizeof(tutorial_model::vertex),
         .usage = granit::buffer_usage::vertex | granit::buffer_usage::transfer_destination});
    if (result.ok()) {
      result = index_buffer_.initialize(
          renderer_owner(),
          {.size = model_.indices.size() * sizeof(std::uint16_t),
           .usage = granit::buffer_usage::index | granit::buffer_usage::transfer_destination});
    }
    granit::upload_batch upload;
    if (result.ok())
      result = upload.initialize(renderer_owner());
    if (result.ok()) {
      result =
          upload.write_buffer(vertex_buffer_.ref(), 0, std::as_bytes(std::span{model_.vertices}));
    }
    if (result.ok())
      result =
          upload.write_buffer(index_buffer_.ref(), 0, std::as_bytes(std::span{model_.indices}));
    if (result.ok())
      result = upload.submit();

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
      result = mesh_.initialize(renderer_owner(),
                                {.topology = granit::primitive_topology::triangle_list,
                                 .vertex_buffers = std::span{&binding, 1},
                                 .index_buffer = index_buffer_.ref(),
                                 .index_format = granit::index_type::uint16,
                                 .index_count = static_cast<std::uint32_t>(model_.indices.size())});
    }
    return result;
  }

  granit::result initialize_materials() noexcept {
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
          renderer_owner(),
          {.format = granit::texture_format::rgba8_unorm,
           .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
           .width = 1,
           .height = 1});
      if (result.ok())
        result = material_textures_[index].write(std::as_bytes(std::span{pixels[index]}), {}, {});
      if (result.ok())
        result = material_views_[index].initialize(renderer_owner(), material_textures_[index]);
    }
    if (result.ok())
      result = material_sampler_.initialize(renderer_owner(), {});

    if (result.ok()) {
      result =
          initialize_material(cube_material_, cube_base_color_, cube_metallic_, cube_roughness_);
    }
    if (result.ok()) {
      result = initialize_material(floor_material_, {0.18F, 0.22F, 0.28F, 1.0F}, 0.0F, 0.82F);
    }
    return result;
  }

  granit::result initialize_material(granit::material_instance& material,
                                     const granit::math::float4& base_color, float metallic,
                                     float roughness) noexcept {
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
    return material.initialize(renderer_owner(), {.archive = material_archive_,
                                                  .initial_updates = updates,
                                                  .shader_library = shader_library_.ref()});
  }

  granit::result update_scene(bool with_renderables) noexcept {
    auto result = scene_.reset();
    if (result.failed())
      return result;

    const auto aspect = static_cast<float>(presentation_info().width) /
                        static_cast<float>(presentation_info().height);
    const auto cube_model = rotation_y(static_cast<float>(rendered_frames()) * 0.02F);
    const auto floor_model = scale_and_translate(3.2F, 0.1F, 3.2F, -1.55F);
    const auto floor_normal = scale_and_translate(1.0F / 3.2F, 10.0F, 1.0F / 3.2F, 0.0F);
    const auto view = view_matrix();
    const auto projection = projection_matrix(aspect);
    const granit::scene_view scene_view{
        .view = view,
        .projection = projection,
        .view_projection = multiply(projection, view),
        .camera_position = {0, 0, 4},
        .viewport_x = 0,
        .viewport_y = 0,
        .viewport_width = static_cast<float>(presentation_info().width),
        .viewport_height = static_cast<float>(presentation_info().height),
        .layer_mask = UINT64_MAX,
    };
    const std::array renderables{
        granit::scene_renderable{.model = cube_model,
                                 .normal_matrix = cube_model,
                                 .bounds_center = {0, 0, 0},
                                 .bounds_radius = 1.0F,
                                 .layer_mask = UINT64_MAX,
                                 .sort_key = 0,
                                 .payload = 1,
                                 .object_id = 1,
                                 .reserved = 0},
        granit::scene_renderable{.model = floor_model,
                                 .normal_matrix = floor_normal,
                                 .bounds_center = {0, -1.55F, 0},
                                 .bounds_radius = 4.53F,
                                 .layer_mask = UINT64_MAX,
                                 .sort_key = 1,
                                 .payload = 2,
                                 .object_id = 2,
                                 .reserved = 0},
    };
    const granit::scene_directional_light light{
        .direction_to_light = {0.365148F, 0.912871F, 0.182574F},
        .radiance = {4.0F, 3.8F, 3.4F},
        .layer_mask = UINT64_MAX,
    };
    const auto visible =
        with_renderables ? std::span{renderables} : std::span<const granit::scene_renderable>{};
    const auto lights = with_renderables ? std::span{&light, 1}
                                         : std::span<const granit::scene_directional_light>{};
    return scene_.initialize(
        renderer_owner(),
        {.views = std::span{&scene_view, 1}, .renderables = visible, .directional_lights = lights});
  }

  granit::result update_cube_material() noexcept {
    const std::array updates{
        granit::material_parameter_update::value(granit::material_parameter_id("base_color"),
                                                 granit::material_parameter_type::float4,
                                                 std::as_bytes(std::span{&cube_base_color_, 1})),
        granit::material_parameter_update::value(granit::material_parameter_id("metallic"),
                                                 granit::material_parameter_type::float32,
                                                 std::as_bytes(std::span{&cube_metallic_, 1})),
        granit::material_parameter_update::value(
            granit::material_parameter_id("perceptual_roughness"),
            granit::material_parameter_type::float32,
            std::as_bytes(std::span{&cube_roughness_, 1})),
    };
    return cube_material_.update(updates);
  }

  granit::result build_imgui_frame(float delta_seconds) noexcept {
    granit::window_state window_state;
    auto result = app_window().get_state(window_state);
    if (result.failed())
      return result;
    tutorial_imgui::begin_frame(window_state, delta_seconds);

    ImGui::SetNextWindowPos({20, 20}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Granit PBR Assets");
    ImGui::Text("Framebuffer: %u x %u", presentation_info().width, presentation_info().height);
    ImGui::TextUnformatted("Render Pipeline -> Tone Mapping -> ImGui Canvas");
    ImGui::Separator();
    bool material_changed = ImGui::ColorEdit3("Base color", &cube_base_color_.x);
    material_changed =
        ImGui::SliderFloat("Metallic", &cube_metallic_, 0.0F, 1.0F) || material_changed;
    material_changed =
        ImGui::SliderFloat("Roughness", &cube_roughness_, 0.04F, 1.0F) || material_changed;
    ImGui::TextUnformatted("Custom Texture ID:");
    ImGui::Image(ImTextureRef{tutorial_imgui::checker_texture_id}, {64, 64});
    ImGui::End();
    ImGui::Render();

    result = material_changed ? update_cube_material() : granit::result::success;
    if (result.ok())
      result = canvas_.clear();
    if (result.ok()) {
      result = granit::integration::imgui::append_draw_data(
          ImGui::GetDrawData(), canvas_, tutorial_imgui::resolve_texture, &imgui_bindings_);
    }
    granit::canvas_draw_list_stats stats{};
    if (result.ok())
      result = canvas_.get_stats(stats);
    if (result.ok())
      canvas_items_ = stats.item_count;
    return result;
  }

  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    last_operation_ = "building ImGui frame";
    auto result = build_imgui_frame(frame.delta_seconds);
    const bool empty_frame = rendered_frames() == 0;
    if (result.ok())
      result = update_scene(!empty_frame);
    if (result.failed())
      return result;

    last_operation_ = "rendering frame";
    const std::array bindings{
        granit::render_pipeline_draw_binding{
            .payload = 1, .mesh = mesh_.ref(), .material = cube_material_.ref()},
        granit::render_pipeline_draw_binding{
            .payload = 2, .mesh = mesh_.ref(), .material = floor_material_.ref()},
    };
    if (result.ok()) {
      granit::render_pipeline_render_desc render_desc{};
      render_desc.scene = scene_.ref();
      render_desc.output = frame.backbuffer.view;
      render_desc.output_format = frame.swapchain.format;
      render_desc.width = frame.swapchain.width;
      render_desc.height = frame.swapchain.height;
      render_desc.draw_bindings = empty_frame
                                      ? std::span<const granit::render_pipeline_draw_binding>{}
                                      : std::span{bindings};
      render_desc.frame = &frame.acquired;
      render_desc.canvas = canvas_.ref();
      render_desc.clear_color = {0.025F, 0.03F, 0.045F, 1.0F};
      result = pipeline_.render(render_desc);
    }
    return result;
  }

  std::vector<std::byte> shader_archive_;
  std::vector<std::byte> material_archive_;
  tutorial_model::mesh_data model_;
  granit::shader_library shader_library_;
  std::array<granit::texture, 5> material_textures_;
  std::array<granit::texture_view, 5> material_views_;
  granit::sampler material_sampler_;
  granit::buffer vertex_buffer_;
  granit::buffer index_buffer_;
  granit::mesh mesh_;
  granit::material_instance cube_material_;
  granit::material_instance floor_material_;
  granit::render_pipeline pipeline_;
  granit::scene_snapshot scene_;
  granit::canvas_draw_list canvas_;
  granit::texture font_texture_;
  granit::texture_view font_view_;
  granit::sampler font_sampler_;
  granit::texture checker_texture_;
  granit::texture_view checker_view_;
  tutorial_imgui::texture_bindings imgui_bindings_;
  granit::math::float4 cube_base_color_{0.82F, 0.24F, 0.08F, 1.0F};
  float cube_metallic_{0.25F};
  float cube_roughness_{0.38F};
  bool imgui_initialized_{};
  std::uint32_t pointer_events_{};
  std::uint32_t canvas_items_{};
  granit::result shutdown_reason_{granit::result::success};
  const char* last_operation_{"starting"};
};

tutorial_application application;

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_02_rendered_frames() noexcept {
  return application.rendered_frames();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_02_recreate_count() noexcept {
  return application.completed_recreates();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_02_pointer_events() noexcept {
  return application.pointer_events();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_02_canvas_items() noexcept {
  return application.canvas_items();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_02_ready() noexcept {
  return application.ready() ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE granit_result granit_tutorial_02_shutdown_reason() noexcept {
  return application.shutdown_reason();
}
#endif

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";

  const auto result = application.run({.title = "Granit PBR Assets",
                                       .application_name = "Granit PBR Assets",
                                       .smoke_test_frames = 5,
                                       .smoke_test = smoke_test});
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  if (result.failed())
    return report_failure("application run", result);
  return 0;
}
