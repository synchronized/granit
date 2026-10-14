// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"
#include "assets/asset_manager.h"
#include "camera/orbit_camera.h"
#include "camera/orbit_camera_input_accumulator.h"
#include "gltf/scene.h"
#include "gltf_rendering/scene_resources.h"
#include "tutorial/tutorial_runtime.h"

#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/resource_table.hpp>
#include <imgui.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace {

int failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << '\n';
  return 1;
}

class application final : public granit::example::application {
public:
  void set_smoke_test(bool value) noexcept { smoke_test_ = value; }
  void set_stress_test(bool value) noexcept { stress_test_ = value; }
  void set_resource_table_stress(bool value) noexcept { resource_table_stress_ = value; }
  void set_pressure_count(std::size_t value) noexcept { pressure_count_ = value; }
  [[nodiscard]] std::uint32_t feature_value() const noexcept {
    return static_cast<std::uint32_t>(model_scene_.materials.size());
  }
  [[nodiscard]] std::uint32_t canvas_items() const noexcept { return runtime_.canvas_items(); }
  [[nodiscard]] granit_result shutdown_reason() const noexcept { return shutdown_reason_.native(); }

private:
  granit::result on_initialize() noexcept override {
    granit::result result = granit::result::success;
    if (!runtime_.initialized()) {
      result = runtime_.initialize(renderer_owner());
      if (result.ok())
        result = renderer_owner().get_limits(limits_);
    }
    if (result.ok() && !model_asset_.valid()) {
      model_asset_ = assets().load<granit::example::gltf::scene>(
          granit::example::assets::asset_location::bundled(
              "tutorials/12_resource_table_scene/ResourceTableGallery.gltf"));
    }
    if (result.ok() && !model_asset_.ready()) {
      if (model_asset_.status() == granit::example::assets::asset_status::failed ||
          model_asset_.status() == granit::example::assets::asset_status::cancelled)
        return granit::result::invalid_argument;
      return granit::result::not_ready;
    }
    if (result.ok()) {
      const auto model_scene = model_asset_.value();
      if (!model_scene || model_scene->materials.empty())
        return granit::result::invalid_argument;
      model_scene_ = *model_scene;
      result = granit::get_resource_table_capabilities(renderer_owner().ref(), table_capabilities_);
      resource_table_available_ =
          table_capabilities_.support == granit::resource_table_support::native;
      model_gpu_.set_resource_table_enabled(resource_table_stress_);
    }
    if (result.ok()) {
      const auto initialize_begin = std::chrono::steady_clock::now();
      result = model_gpu_.initialize(renderer_owner(), model_scene_);
      scene_initialize_elapsed_us_ =
          static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                         std::chrono::steady_clock::now() - initialize_begin)
                                         .count());
    }
    if (result.ok())
      result = pipeline_.initialize(renderer(), {.enable_fxaa = true});
    if (result.ok() && stress_test_) {
      const auto metrics_result = pipeline_.enable_metrics();
      gpu_metrics_enabled_ = metrics_result.ok();
    }
    if (result.ok() && resource_table_stress_ && resource_table_available_) {
      result = table_.initialize(
          renderer_owner(),
          {.resource_types = GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW_BIT |
                             GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT,
           .sampled_texture_view_capacity =
               static_cast<std::uint32_t>(model_scene_.materials.size()),
           .sampler_capacity = 1});
    }
    if (result.ok() && resource_table_stress_ && resource_table_available_) {
      result = sampler_.initialize(renderer_owner());
    }
    if (result.ok() && resource_table_stress_ && resource_table_available_) {
      result = table_.update({.type = granit::resource_table_resource_type::sampler,
                              .slot = 0,
                              .resource = sampler_.native_handle()});
      if (result.ok())
        ++resource_table_update_count_;
    }
    if (result.ok() && resource_table_stress_ && resource_table_available_) {
      std::vector<granit::resource_table_update> updates;
      updates.reserve(model_scene_.materials.size() + 1);
      for (std::size_t index = 0; index < model_scene_.materials.size(); ++index) {
        granit::texture_view_ref texture_view;
        granit::sampler_ref texture_sampler;
        result = model_gpu_.texture_binding(model_scene_.materials[index].base_color_texture, true,
                                            texture_view, texture_sampler);
        if (result.failed())
          break;
        const auto texture = texture_view.native_handle();
        if (index == 0)
          resource_table_texture_ = texture;
        updates.push_back({.type = granit::resource_table_resource_type::sampled_texture_view,
                           .slot = static_cast<std::uint32_t>(index),
                           .resource = texture});
        if (index == 0)
          resource_table_sampler_ = texture_sampler.native_handle();
      }
      if (result.ok()) {
        updates.push_back({.type = granit::resource_table_resource_type::sampler,
                           .slot = 0,
                           .resource = resource_table_sampler_});
        result = table_.update_batch(updates);
        if (result.ok())
          resource_table_update_count_ += updates.size();
      }
    }
    if (result.ok() && stress_test_ && resource_table_stress_ && resource_table_available_) {
      const auto stress_begin = std::chrono::steady_clock::now();
      stress_tables_.resize(pressure_count_);
      for (auto& table : stress_tables_) {
        result = table.initialize(
            renderer_owner(),
            {.resource_types = GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT, .sampler_capacity = 1});
        if (result.failed())
          break;
        result = table.update({.type = granit::resource_table_resource_type::sampler,
                               .slot = 0,
                               .resource = sampler_.native_handle()});
        if (result.ok())
          ++resource_table_update_count_;
        if (result.failed())
          break;
      }
      if (result.ok()) {
        stress_elapsed_us_ =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                           std::chrono::steady_clock::now() - stress_begin)
                                           .count());
        std::cout << "resource_table_pressure tables=" << stress_tables_.size()
                  << " sampler_updates=" << stress_tables_.size()
                  << " elapsed_us=" << stress_elapsed_us_ << '\n';
      }
    } else if (result.ok() && stress_test_ && !resource_table_stress_) {
      const auto stress_begin = std::chrono::steady_clock::now();
      granit::texture_view_ref texture_view;
      granit::sampler_ref texture_sampler;
      result = model_gpu_.texture_binding(model_scene_.materials.front().base_color_texture, true,
                                          texture_view, texture_sampler);
      if (result.ok()) {
        const std::array layout_entries{
            granit::bind_group_layout_entry{.binding = 0,
                                            .type = granit::binding_type::sampled_texture,
                                            .array_count = 1,
                                            .visibility = granit::shader_stage_flags::fragment},
            granit::bind_group_layout_entry{.binding = 1,
                                            .type = granit::binding_type::sampler,
                                            .array_count = 1,
                                            .visibility = granit::shader_stage_flags::fragment}};
        result = pressure_bind_group_layout_.initialize(renderer_owner(), layout_entries);
        if (result.ok()) {
          pressure_bind_groups_.resize(pressure_count_);
          for (auto& group : pressure_bind_groups_) {
            const std::array entries{
                granit::bind_group_entry{.binding = 0, .resource = texture_view},
                granit::bind_group_entry{.binding = 1, .resource = texture_sampler}};
            result = group.initialize(renderer(), pressure_bind_group_layout_.ref(), entries);
            if (result.failed())
              break;
          }
        }
      }
      bind_group_pressure_elapsed_us_ =
          static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                         std::chrono::steady_clock::now() - stress_begin)
                                         .count());
      if (result.ok()) {
        std::cout << "bind_group_pressure groups=" << pressure_bind_groups_.size()
                  << " resource_entries=" << pressure_bind_groups_.size() * 2
                  << " elapsed_us=" << bind_group_pressure_elapsed_us_ << '\n';
      }
    }
    if (result.ok() && stress_test_) {
      const auto mode = resource_table_stress_ ? "resource_table" : "bind_group";
      const auto bind_group_materials =
          resource_table_stress_ ? 0U : static_cast<unsigned>(model_scene_.materials.size());
      std::cout << "binding_pressure mode=" << mode << " pressure_count=" << pressure_count_
                << " materials=" << model_scene_.materials.size()
                << " draws=" << model_gpu_.draw_bindings().size()
                << " bind_group_materials=" << bind_group_materials
                << " resource_table_updates=" << resource_table_update_count_
                << " bind_group_pressure_groups=" << pressure_bind_groups_.size()
                << " scene_initialize_us=" << scene_initialize_elapsed_us_ << '\n';
    }
    if (result.ok() &&
        !camera_.focus({.radius = 1.6F}, presentation_info().width, presentation_info().height))
      result = granit::result::invalid_argument;
    return result;
  }

  granit::result on_swapchain_changed(const granit::swapchain_info&) noexcept override {
    return granit::result::success;
  }

  void on_shutdown(granit::result reason) noexcept override {
    shutdown_reason_ = reason;
    runtime_.shutdown();
    static_cast<void>(scene_.reset());
    static_cast<void>(pipeline_.reset());
    pressure_bind_groups_.clear();
    static_cast<void>(pressure_bind_group_layout_.reset());
    model_gpu_.reset();
  }

  granit::result on_window_event(const granit::window_event& event) noexcept override {
    runtime_.process(event);
    return granit::result::success;
  }

  granit::result on_input_event(const granit::input_event& event) noexcept override {
    runtime_.process(event);
    camera_input_.process(event, runtime_.wants_mouse(), runtime_.wants_keyboard());
    return granit::result::success;
  }

  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    publish_gpu_metrics();
    camera_input_.begin_frame();
    granit::window_state state;
    auto result = app_window().get_state(state);
    if (result.ok()) {
      result = runtime_.begin_frame(state, frame.delta_seconds,
                                    {.name = "12 Resource Table Scene",
                                     .description = "Resource Table scene and pressure validation",
                                     .frame = rendered_frames()});
    }
    if (result.ok()) {
      ImGui::Text("Descriptor Indexing device support: %s",
                  limits_.supports_bindless_descriptor_indexing() ? "yes" : "no");
      ImGui::Text("Resource Table support: %s",
                  resource_table_support_name(table_capabilities_.support));
      ImGui::Text("Resource Table sampler slot 0: %s",
                  resource_table_available_ ? "populated" : "unavailable");
      ImGui::Text("Pressure tables: %zu", stress_tables_.size());
      ImGui::Text("Gallery materials: %zu", model_scene_.materials.size());
      ImGui::Text("Resource Table texture slot 0: %s",
                  resource_table_texture_ != GRANIT_NULL_HANDLE ? "populated" : "empty");
      if (resource_table_available_)
        ImGui::Text("Resource Table texture capacity: %zu", model_scene_.materials.size());
      else
        ImGui::TextUnformatted("Resource Table texture capacity: unavailable");
      if (stress_test_)
        ImGui::Text("Stress mode: %zu tables, %zu sampler updates, %llu us", stress_tables_.size(),
                    stress_tables_.size(), static_cast<unsigned long long>(stress_elapsed_us_));
      ImGui::Text("Requested path: %s", resource_table_stress_
                                            ? "Resource Table (unsupported devices fall back)"
                                            : "Bind Group");
      ImGui::Text("Material implementation selection is explicit");
      result = runtime_.end_frame();
    }
    if (result.failed())
      return result;
    const auto input = camera_input_.finish(runtime_.wants_mouse(), runtime_.wants_keyboard());
    if (!smoke_test_ && !camera_.orbit(frame.delta_seconds * 0.25F))
      result = granit::result::invalid_argument;
    if (result.ok() && !camera_.update(input, frame.swapchain.width, frame.swapchain.height))
      result = granit::result::invalid_argument;
    const bool empty_frame = rendered_frames() == 0;
    if (result.ok())
      result = update_scene(!empty_frame);
    if (result.failed())
      return result;
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
    render_desc.canvas = runtime_.canvas_ref();
    render_desc.clear_color = {0.025F, 0.03F, 0.045F, 1.0F};
    return pipeline_.render(render_desc);
  }

  granit::result update_scene(bool with_renderables) noexcept {
    auto result = scene_.reset();
    if (result.failed())
      return result;
    granit::example::camera::camera_matrices matrices;
    if (!camera_.matrices(presentation_info().width, presentation_info().height, matrices))
      return granit::result::invalid_argument;
    const granit::scene_view scene_view{
        .view = matrices.view,
        .projection = matrices.projection,
        .view_projection = matrices.view_projection,
        .camera_position = matrices.position,
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
    if (with_renderables)
      return model_gpu_.create_snapshot(std::span{&scene_view, 1}, std::span{&light, 1}, {}, {},
                                        scene_);
    return scene_.initialize(renderer_owner(), {.views = std::span{&scene_view, 1}});
  }

  void publish_gpu_metrics() noexcept {
    if (!stress_test_ || !gpu_metrics_enabled_)
      return;
    granit::render_pipeline_metrics metrics;
    if (pipeline_.get_metrics(metrics).failed() || metrics.sample_sequence == gpu_sample_sequence_)
      return;
    gpu_sample_sequence_ = metrics.sample_sequence;
    std::cout << "binding_pressure_gpu mode="
              << (resource_table_stress_ ? "resource_table" : "bind_group")
              << " sample=" << metrics.sample_sequence << " shadow_gpu_ns=" << metrics.shadow_gpu_ns
              << " opaque_gpu_ns=" << metrics.opaque_gpu_ns
              << " tone_mapping_gpu_ns=" << metrics.tone_mapping_gpu_ns
              << " total_gpu_ns=" << metrics.total_gpu_ns << '\n';
  }

  granit::example::tutorial::tutorial_runtime runtime_;
  granit::renderer_limits limits_{};
  granit::resource_table_capabilities table_capabilities_{};
  bool resource_table_available_{};
  granit::resource_table table_;
  std::vector<granit::resource_table> stress_tables_;
  granit::bind_group_layout pressure_bind_group_layout_;
  std::vector<granit::bind_group> pressure_bind_groups_;
  granit::sampler sampler_;
  granit::example::assets::asset_handle<granit::example::gltf::scene> model_asset_;
  granit::example::gltf::scene model_scene_;
  granit::example::gltf_rendering::scene_resources model_gpu_;
  granit::render_pipeline pipeline_;
  granit::scene_snapshot scene_;
  granit::example::camera::orbit_camera camera_;
  granit::example::camera::orbit_camera_input_accumulator camera_input_;
  granit_handle resource_table_texture_{GRANIT_NULL_HANDLE};
  granit_handle resource_table_sampler_{GRANIT_NULL_HANDLE};
  std::uint64_t stress_elapsed_us_{};
  std::uint64_t bind_group_pressure_elapsed_us_{};
  std::uint64_t scene_initialize_elapsed_us_{};
  std::size_t resource_table_update_count_{};
  std::uint64_t gpu_sample_sequence_{};
  bool gpu_metrics_enabled_{};
  bool smoke_test_{};
  bool stress_test_{};
  bool resource_table_stress_{true};
  std::size_t pressure_count_{64};
  granit::result shutdown_reason_{granit::result::success};

  static const char* resource_table_support_name(granit::resource_table_support support) noexcept {
    switch (support) {
    case granit::resource_table_support::native:
      return "native";
    case granit::resource_table_support::logical:
      return "logical";
    case granit::resource_table_support::fallback:
      return "fallback";
    default:
      return "unsupported";
    }
  }
};

application app;
} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_12_rendered_frames() noexcept {
  return app.rendered_frames();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_12_recreate_count() noexcept {
  return app.completed_recreates();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_12_feature_value() noexcept {
  return app.feature_value();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_12_canvas_items() noexcept {
  return app.canvas_items();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_12_ready() noexcept {
  return app.ready() ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE granit_result granit_tutorial_12_shutdown_result() noexcept {
  return app.shutdown_reason();
}
#endif

int main(int argc, char** argv) {
  const auto argument = argc >= 2 ? std::string_view{argv[1]} : std::string_view{};
  const bool smoke = argument == "--smoke-test";
  const bool resource_table_stress = argument == "--stress-test" || argument == "--stress-test-4" ||
                                     argument == "--stress-test-16" ||
                                     argument == "--stress-test-32" ||
                                     argument == "--stress-test-64";
  const bool bind_group_stress =
      argument == "--bind-group-stress-test" || argument == "--bind-group-stress-test-4" ||
      argument == "--bind-group-stress-test-16" || argument == "--bind-group-stress-test-32" ||
      argument == "--bind-group-stress-test-64";
  const bool stress = resource_table_stress || bind_group_stress;
  app.set_smoke_test(smoke || stress);
  app.set_stress_test(stress);
  app.set_resource_table_stress(!bind_group_stress);
  if (argument.ends_with("-4"))
    app.set_pressure_count(4);
  else if (argument.ends_with("-16"))
    app.set_pressure_count(16);
  else if (argument.ends_with("-32"))
    app.set_pressure_count(32);
  else
    app.set_pressure_count(64);
  const auto result = app.run({.executable_path = argc > 0 ? argv[0] : "",
                               .title = "Granit Resource Table Scene",
                               .renderer = {.application_name = "Granit Resource Table",
                                            .presentation = granit::presentation_mode::enabled},
                               .swapchain = {},
                               .smoke_test = smoke || stress});
  if (smoke && result == granit::result::backend_unavailable)
    return 77;
  return result.failed() ? failure("application run", result) : 0;
}
