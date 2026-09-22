// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>
#include <granit/integrations/imgui/renderer.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/material.hpp>
#include <granit/pipeline/mesh.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/window.hpp>
#include <imgui.h>

#include <array>
#include <chrono>
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

#ifndef GRANIT_TUTORIAL_08_SHADER_LIBRARY
#error "GRANIT_TUTORIAL_08_SHADER_LIBRARY must point to the generated Shader Library"
#endif
#ifndef GRANIT_TUTORIAL_08_MATERIAL
#error "GRANIT_TUTORIAL_08_MATERIAL must point to the generated Material archive"
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

granit::result poll_window_events(granit::window_system& window_system, bool& running,
                                  bool& recreate) {
  granit::window_event event;
  granit::result result;
  while ((result = window_system.poll(event)).ok()) {
    tutorial_imgui::process_window_event(event);
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

granit::result poll_input_events(granit::window_system& window_system, bool& running,
                                 std::uint32_t& pointer_events) {
  granit::input_event event;
  granit::result result;
  while ((result = window_system.poll(event)).ok()) {
    tutorial_imgui::process_input_event(event);
    if (event.type == granit::input_event_type::pointer_moved ||
        event.type == granit::input_event_type::pointer_button ||
        event.type == granit::input_event_type::pointer_wheel) {
      ++pointer_events;
    }
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
    shader_archive_ = read_file(GRANIT_TUTORIAL_08_SHADER_LIBRARY);
    material_archive_ = read_file(GRANIT_TUTORIAL_08_MATERIAL);
    if (shader_archive_.empty() || material_archive_.empty()) {
      std::cerr << "Failed to read generated Shader Library or Material archive\n";
      return granit::result::invalid_argument;
    }

    auto result = window_system_.initialize();
    if (result.failed())
      return result;
    result = window_.initialize(window_system_,
                                {.title = "Granit Tutorial 08", .width = 1280, .height = 720});
    if (result.failed())
      return result;
    result = renderer_.initialize({.application_name = "Granit Tutorial 08",
                                   .presentation = granit::presentation_mode::enabled});
    if (result.ok()) {
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGui::GetIO().IniFilename = nullptr;
      imgui_initialized_ = true;
      previous_frame_time_ = std::chrono::steady_clock::now();
    }
    return result;
  }

  granit::result tick(granit::window_loop_action& action) noexcept {
    auto result = poll_window_events(window_system_, running_, recreate_);
    if (result.ok())
      result = poll_input_events(window_system_, running_, pointer_events_);
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

    last_operation_ = "building ImGui frame";
    result = build_imgui_frame();
    if (result.ok()) {
      last_operation_ = "rendering frame";
      result = render_frame();
    }
    if (result == granit::result::out_of_date) {
      recreate_ = true;
      return granit::result::success;
    }
    if (result.failed())
      return result;

    ++rendered_frames_;
    if (smoke_test_ && (rendered_frames_ == 1 || rendered_frames_ == 3))
      recreate_ = true;
    if (smoke_complete())
      action = granit::window_loop_action::stop;
    return granit::result::success;
  }

  void shutdown(granit::result reason) noexcept {
    shutdown_reason_ = reason;
    if (reason.failed())
      std::cerr << "window loop stopped during " << last_operation_ << ": " << reason.message()
                << '\n';
    phase_ = application_phase::stopped;
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
    static_cast<void>(swapchain_.reset());
    static_cast<void>(surface_.reset());
    static_cast<void>(renderer_.reset());
    static_cast<void>(window_.reset());
    static_cast<void>(window_system_.reset());
  }

  [[nodiscard]] granit::window_system& system() noexcept { return window_system_; }
  [[nodiscard]] std::uint32_t rendered_frames() const noexcept { return rendered_frames_; }
  [[nodiscard]] std::uint32_t completed_recreates() const noexcept { return completed_recreates_; }
  [[nodiscard]] std::uint32_t pointer_events() const noexcept { return pointer_events_; }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return canvas_items_; }
  [[nodiscard]] bool ready() const noexcept { return phase_ == application_phase::running; }
  [[nodiscard]] granit_result shutdown_reason() const noexcept { return shutdown_reason_.native(); }

private:
  granit::result initialize_presentation() noexcept {
    last_operation_ = "querying window state";
    auto result = window_.get_state(window_state_);
    if (result.failed())
      return result;
    if (window_state_.framebuffer_width == 0 || window_state_.framebuffer_height == 0)
      return granit::result::not_ready;
    last_operation_ = "creating surface";
    result = window_.create_surface(renderer_, surface_);
    if (result.ok()) {
      last_operation_ = "creating swapchain";
      result = swapchain_.initialize(
          renderer_, surface_,
          {.width = window_state_.framebuffer_width, .height = window_state_.framebuffer_height});
    }
    if (result.ok()) {
      last_operation_ = "querying swapchain";
      result = swapchain_.query_info(swapchain_info_);
    }
    if (result.ok()) {
      last_operation_ = "loading shader library";
      result = shader_library_.initialize(renderer_, shader_archive_);
    }
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
      result = pipeline_.initialize(renderer_, {.enable_fxaa = true, .enable_specular_aa = true});
    }
    if (result.ok()) {
      last_operation_ = "creating canvas";
      result = canvas_.initialize(renderer_);
    }
    if (result.ok()) {
      last_operation_ = "uploading ImGui font";
      result =
          tutorial_imgui::upload_font_atlas(renderer_, font_texture_, font_view_, font_sampler_);
    }
    if (result.ok()) {
      last_operation_ = "uploading checker texture";
      result = tutorial_imgui::upload_checker(renderer_, checker_texture_, checker_view_);
    }
    if (result.ok()) {
      imgui_bindings_ = {.font = {font_view_.ref(), font_sampler_.ref()},
                         .checker = {checker_view_.ref(), font_sampler_.ref()}};
    }
    return result;
  }

  granit::result initialize_geometry() noexcept {
    auto result =
        vertex_buffer_.initialize(renderer_, {.size = sizeof(tutorial_model::vertices),
                                              .usage = granit::buffer_usage::vertex |
                                                       granit::buffer_usage::transfer_destination});
    if (result.ok()) {
      result = index_buffer_.initialize(
          renderer_,
          {.size = sizeof(tutorial_model::indices),
           .usage = granit::buffer_usage::index | granit::buffer_usage::transfer_destination});
    }
    granit::upload_batch upload;
    if (result.ok())
      result = upload.initialize(renderer_);
    if (result.ok()) {
      result = upload.write_buffer(vertex_buffer_.ref(), 0,
                                   std::as_bytes(std::span{tutorial_model::vertices}));
    }
    if (result.ok())
      result = upload.write_buffer(index_buffer_.ref(), 0,
                                   std::as_bytes(std::span{tutorial_model::indices}));
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
      result = mesh_.initialize(
          renderer_, {.topology = granit::primitive_topology::triangle_list,
                      .vertex_buffers = std::span{&binding, 1},
                      .index_buffer = index_buffer_.ref(),
                      .index_format = granit::index_type::uint16,
                      .index_count = static_cast<std::uint32_t>(tutorial_model::indices.size())});
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
    return material.initialize(renderer_, {.archive = material_archive_,
                                           .initial_updates = updates,
                                           .shader_library = shader_library_.ref()});
  }

  granit::result update_scene(bool with_renderables) noexcept {
    auto result = scene_.reset();
    if (result.failed())
      return result;

    const auto aspect =
        static_cast<float>(swapchain_info_.width) / static_cast<float>(swapchain_info_.height);
    const auto cube_model = rotation_y(static_cast<float>(rendered_frames_) * 0.02F);
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
        .viewport_width = static_cast<float>(swapchain_info_.width),
        .viewport_height = static_cast<float>(swapchain_info_.height),
        .layer_mask = UINT64_MAX,
    };
    const std::array renderables{
        granit::scene_renderable{.model = cube_model,
                                 .normal_matrix = cube_model,
                                 .bounds_center = {0, 0, 0},
                                 .bounds_radius = 1.7320508F,
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
        renderer_,
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

  granit::result build_imgui_frame() noexcept {
    const auto now = std::chrono::steady_clock::now();
    const auto delta = std::chrono::duration<float>(now - previous_frame_time_).count();
    previous_frame_time_ = now;
    tutorial_imgui::begin_frame(window_state_, delta);

    ImGui::SetNextWindowPos({20, 20}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Granit Tutorial 08");
    ImGui::Text("Framebuffer: %u x %u", swapchain_info_.width, swapchain_info_.height);
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

    auto result = material_changed ? update_cube_material() : granit::result::success;
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

  granit::result render_frame() noexcept {
    const bool empty_frame = smoke_test_ && rendered_frames_ == 0;
    auto result = update_scene(!empty_frame);
    if (result.failed())
      return result;

    granit::acquired_frame frame;
    result = swapchain_.acquire(frame);
    if (result.failed())
      return result;
    recreate_ = recreate_ || frame.needs_recreate();

    granit::swapchain_backbuffer backbuffer;
    result = swapchain_.backbuffer(frame, backbuffer);
    const std::array bindings{
        granit::render_pipeline_draw_binding{
            .payload = 1, .mesh = mesh_.ref(), .material = cube_material_.ref()},
        granit::render_pipeline_draw_binding{
            .payload = 2, .mesh = mesh_.ref(), .material = floor_material_.ref()},
    };
    if (result.ok()) {
      granit::render_pipeline_render_desc render_desc{};
      render_desc.scene = scene_.ref();
      render_desc.output = backbuffer.view;
      render_desc.output_format = swapchain_info_.format;
      render_desc.width = swapchain_info_.width;
      render_desc.height = swapchain_info_.height;
      render_desc.draw_bindings = empty_frame
                                      ? std::span<const granit::render_pipeline_draw_binding>{}
                                      : std::span{bindings};
      render_desc.frame = &frame;
      render_desc.canvas = canvas_.ref();
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
    return smoke_test_ && rendered_frames_ >= 5 && completed_recreates_ >= 2;
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
  std::chrono::steady_clock::time_point previous_frame_time_{};
  granit::window_state window_state_{};
  granit::swapchain_info swapchain_info_{};
  application_phase phase_{application_phase::renderer_initializing};
  bool running_{true};
  bool recreate_{};
  bool smoke_test_{};
  bool imgui_initialized_{};
  std::uint32_t rendered_frames_{};
  std::uint32_t completed_recreates_{};
  std::uint32_t pointer_events_{};
  std::uint32_t canvas_items_{};
  granit::result shutdown_reason_{granit::result::success};
  const char* last_operation_{"starting"};
};

tutorial_application application;

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_08_rendered_frames() noexcept {
  return application.rendered_frames();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_08_recreate_count() noexcept {
  return application.completed_recreates();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_08_pointer_events() noexcept {
  return application.pointer_events();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_08_canvas_items() noexcept {
  return application.canvas_items();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_08_ready() noexcept {
  return application.ready() ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE granit_result granit_tutorial_08_shutdown_reason() noexcept {
  return application.shutdown_reason();
}
#endif

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
