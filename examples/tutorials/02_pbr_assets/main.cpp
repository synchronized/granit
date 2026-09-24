// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "assets/asset_store_resolver.h"
#include "gltf/loader.h"
#include "imgui/imgui_font_atlas.h"
#include "imgui/imgui_input.h"
#include "imgui/imgui_texture_registry.h"
#include "model_viewer/gpu_scene.h"

#include <granit/integrations/imgui/renderer.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
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

class tutorial_application final : public granit::example::application {
public:
  [[nodiscard]] std::uint32_t pointer_events() const noexcept { return pointer_events_; }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return canvas_items_; }
  [[nodiscard]] granit_result shutdown_reason() const noexcept { return shutdown_reason_.native(); }

private:
  granit::result on_initialize() noexcept override {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    imgui_initialized_ = true;

    last_operation_ = "loading Suzanne glTF";
    auto result = initialize_model();
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
      result = granit::example::imgui::initialize_font_atlas(
          renderer_owner(), imgui_textures_, font_texture_, font_view_, font_sampler_);
    }
    if (result.ok()) {
      granit::texture_view_ref preview_view;
      granit::sampler_ref preview_sampler;
      result = model_gpu_.texture_binding(model_scene_.materials.front().base_color_texture, true,
                                          preview_view, preview_sampler);
      if (result.ok())
        result =
            imgui_textures_.register_texture(preview_view, preview_sampler, preview_texture_id_);
    }
    return result;
  }

  granit::result on_window_event(const granit::window_event& event) noexcept override {
    granit::example::imgui::process_window_event(event);
    return granit::result::success;
  }

  granit::result on_input_event(const granit::input_event& event) noexcept override {
    granit::example::imgui::process_input_event(event);
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
    imgui_textures_.clear();
    static_cast<void>(scene_.reset());
    static_cast<void>(pipeline_.reset());
    static_cast<void>(canvas_.destroy());
    model_gpu_.reset();
    static_cast<void>(font_view_.reset());
    static_cast<void>(font_texture_.reset());
    static_cast<void>(font_sampler_.reset());
    if (imgui_initialized_) {
      ImGui::DestroyContext();
      imgui_initialized_ = false;
    }
  }

  granit::result initialize_model() noexcept {
    std::vector<std::byte> document;
    if (!assets().read("tutorials/02_pbr_assets/Suzanne.gltf", document)) {
      std::cerr << "Failed to read Suzanne model\n";
      return granit::result::invalid_argument;
    }

    granit::example::assets::asset_store_resolver resolver{assets(), "tutorials/02_pbr_assets"};
    const auto loaded = granit::example::gltf::load(document, &resolver, model_scene_);
    if (!loaded) {
      std::cerr << "Failed to load Suzanne model: " << loaded.diagnostic << '\n';
      return granit::result::invalid_argument;
    }
    if (model_scene_.materials.empty()) {
      std::cerr << "Suzanne model does not contain a PBR material\n";
      return granit::result::invalid_argument;
    }

    const auto& material = model_scene_.materials.front();
    model_base_color_ = material.base_color;
    model_metallic_ = material.metallic;
    model_roughness_ = material.roughness;
    return model_gpu_.initialize(renderer_owner(), model_scene_);
  }
  granit::result update_scene(bool with_renderables) noexcept {
    auto result = scene_.reset();
    if (result.failed())
      return result;

    const auto aspect = static_cast<float>(presentation_info().width) /
                        static_cast<float>(presentation_info().height);
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
    const granit::scene_directional_light light{
        .direction_to_light = {0.365148F, 0.912871F, 0.182574F},
        .radiance = {4.0F, 3.8F, 3.4F},
        .layer_mask = UINT64_MAX,
    };
    if (with_renderables) {
      return model_gpu_.create_snapshot(std::span{&scene_view, 1}, std::span{&light, 1}, {}, {},
                                        scene_);
    }
    return scene_.initialize(renderer_owner(), {.views = std::span{&scene_view, 1}});
  }

  granit::result update_model_material() noexcept {
    const auto& material = model_scene_.materials.front();
    const granit::example::model_viewer::material_factor_edit edit{
        .base_color = model_base_color_,
        .metallic = model_metallic_,
        .roughness = model_roughness_,
        .normal_scale = material.normal_scale,
        .occlusion_strength = material.occlusion_strength,
        .emissive = material.emissive,
    };
    return model_gpu_.update_material_factors(model_scene_, 0, edit);
  }
  granit::result build_imgui_frame(float delta_seconds) noexcept {
    granit::window_state window_state;
    auto result = app_window().get_state(window_state);
    if (result.failed())
      return result;
    granit::example::imgui::begin_frame(window_state, delta_seconds);

    ImGui::SetNextWindowPos({20, 20}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Granit PBR Assets");
    ImGui::Text("Framebuffer: %u x %u", presentation_info().width, presentation_info().height);
    ImGui::TextUnformatted("Render Pipeline -> Tone Mapping -> ImGui Canvas");
    ImGui::Separator();
    bool material_changed = ImGui::ColorEdit3("Base color", &model_base_color_.x);
    material_changed =
        ImGui::SliderFloat("Metallic", &model_metallic_, 0.0F, 1.0F) || material_changed;
    material_changed =
        ImGui::SliderFloat("Roughness", &model_roughness_, 0.04F, 1.0F) || material_changed;
    ImGui::TextUnformatted("Suzanne base color texture:");
    ImGui::Image(ImTextureRef{preview_texture_id_}, {64, 64});
    ImGui::End();
    ImGui::Render();

    result = material_changed ? update_model_material() : granit::result::success;
    if (result.ok())
      result = canvas_.clear();
    if (result.ok()) {
      result = granit::integration::imgui::append_draw_data(
          ImGui::GetDrawData(), canvas_, granit::example::imgui::texture_registry::resolver,
          &imgui_textures_);
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
    if (result.ok()) {
      granit::render_pipeline_render_desc render_desc{};
      render_desc.scene = scene_.ref();
      render_desc.output = frame.backbuffer.view;
      render_desc.output_format = frame.swapchain.format;
      render_desc.width = frame.swapchain.width;
      render_desc.height = frame.swapchain.height;
      render_desc.draw_bindings = empty_frame
                                      ? std::span<const granit::render_pipeline_draw_binding>{}
                                      : std::span{model_gpu_.draw_bindings()};
      render_desc.frame = &frame.acquired;
      render_desc.canvas = canvas_.ref();
      render_desc.clear_color = {0.025F, 0.03F, 0.045F, 1.0F};
      result = pipeline_.render(render_desc);
    }
    return result;
  }

  granit::example::gltf::scene model_scene_;
  granit::example::model_viewer::gpu_scene model_gpu_;
  granit::render_pipeline pipeline_;
  granit::scene_snapshot scene_;
  granit::canvas_draw_list canvas_;
  granit::texture font_texture_;
  granit::texture_view font_view_;
  granit::sampler font_sampler_;
  granit::example::imgui::texture_registry imgui_textures_;
  ImTextureID preview_texture_id_{ImTextureID_Invalid};
  granit::math::float4 model_base_color_{1.0F, 1.0F, 1.0F, 1.0F};
  float model_metallic_{1.0F};
  float model_roughness_{1.0F};
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
  const std::string_view executable_path =
      argument_count > 0 && arguments[0] != nullptr ? arguments[0] : "";

  const auto result = application.run({.executable_path = executable_path,
                                       .title = "Granit PBR Assets",
                                       .application_name = "Granit PBR Assets",
                                       .smoke_test_frames = 5,
                                       .smoke_test = smoke_test});
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  if (result.failed())
    return report_failure("application run", result);
  return 0;
}
