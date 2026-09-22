// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_RENDER_PIPELINE_HPP_
#define GRANIT_PIPELINE_RENDER_PIPELINE_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/debug_draw_list.hpp>
#include <granit/pipeline/material.hpp>
#include <granit/pipeline/mesh.hpp>
#include <granit/pipeline/render_pipeline.h>
#include <granit/pipeline/scene.hpp>
#include <granit/renderer/render_target.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/resource_types.hpp>
#include <granit/renderer/swapchain.hpp>
#include <granit/renderer/texture.hpp>

#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace granit {

enum class render_pipeline_stage : std::uint32_t {
  opaque = GRANIT_RENDER_PIPELINE_STAGE_OPAQUE,
  shadow = GRANIT_RENDER_PIPELINE_STAGE_SHADOW,
  overlay = GRANIT_RENDER_PIPELINE_STAGE_OVERLAY,
};

struct render_pipeline_draw_binding {
  std::uint64_t payload{};
  mesh_ref mesh;
  material_instance_ref material;
};

struct render_pipeline_desc {
  granit_render_pipeline_record_callback record{};
  void* user_data{};
  sample_count samples{sample_count::one};
  bool enable_fxaa{true};
  bool enable_specular_aa{true};
};

struct render_pipeline_output {
  texture_view_ref view;
  texture_format format{texture_format::undefined};
  std::uint32_t width{};
  std::uint32_t height{};
  canvas_draw_list_ref canvas;
  debug_draw_list_ref debug_draw;
};

struct render_pipeline_environment {
  texture_view_ref irradiance;
  texture_view_ref prefiltered_environment;
  texture_view_ref brdf_lut;
  float rotation_radians{};
  float intensity{1.0F};
  float prefiltered_max_mip{};
};

struct render_pipeline_render_desc {
  scene_snapshot_ref scene;
  texture_view_ref output;
  texture_format output_format{texture_format::undefined};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t first_view{};
  std::uint32_t view_count{1};
  float exposure_ev{};
  std::span<const render_pipeline_draw_binding> draw_bindings;
  std::span<const render_pipeline_output> outputs;
  const acquired_frame* frame{};
  canvas_draw_list_ref canvas;
  debug_draw_list_ref debug_draw;
  clear_color_value clear_color{0.0F, 0.0F, 0.0F, 1.0F};
  const render_pipeline_environment* environment{};
};

struct render_pipeline_metrics {
  std::uint64_t sample_sequence{};
  std::uint64_t shadow_gpu_ns{};
  std::uint64_t opaque_gpu_ns{};
  std::uint64_t tone_mapping_gpu_ns{};
  std::uint64_t total_gpu_ns{};
};

class render_pipeline;

/** 统一参考渲染管线 C ABI 的轻量 move-only RAII 包装。 */
class render_pipeline {
public:
  render_pipeline() = default;
  ~render_pipeline() { static_cast<void>(reset()); }
  render_pipeline(const render_pipeline&) = delete;
  render_pipeline& operator=(const render_pipeline&) = delete;
  render_pipeline(render_pipeline&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  render_pipeline& operator=(render_pipeline&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const granit_render_pipeline_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_render_pipeline_create(renderer, &desc, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result initialize(renderer& owner, const render_pipeline_desc& desc = {}) noexcept {
    const granit_render_pipeline_desc native{
        .struct_size = GRANIT_RENDER_PIPELINE_DESC_VERSION_1_SIZE,
        .reserved = 0,
        .record = desc.record,
        .user_data = desc.user_data,
        .sample_count = static_cast<granit_sample_count>(desc.samples),
        .enable_fxaa = desc.enable_fxaa ? 1U : 0U,
        .enable_specular_aa = desc.enable_specular_aa ? 1U : 0U,
        .reserved_2 = 0,
    };
    return initialize(owner.native_handle(), native);
  }
  [[nodiscard]] result render(const granit_render_pipeline_render_desc& desc) noexcept {
    return from_native(granit_render_pipeline_render(renderer_, handle_, &desc));
  }
  [[nodiscard]] result render(const render_pipeline_render_desc& desc) noexcept {
    if (desc.draw_bindings.size() > std::numeric_limits<std::uint32_t>::max() ||
        desc.outputs.size() > std::numeric_limits<std::uint32_t>::max()) {
      return result::invalid_argument;
    }
    try {
      std::vector<granit_render_pipeline_draw_binding> bindings;
      bindings.reserve(desc.draw_bindings.size());
      for (const auto& binding : desc.draw_bindings) {
        bindings.push_back({.payload = binding.payload,
                            .mesh = binding.mesh.native_handle(),
                            .material = binding.material.native_handle(),
                            .reserved = 0});
      }
      std::vector<granit_render_pipeline_output> outputs;
      outputs.reserve(desc.outputs.size());
      for (const auto& output : desc.outputs)
        outputs.push_back(to_native(output));

      granit_render_pipeline_environment environment = GRANIT_RENDER_PIPELINE_ENVIRONMENT_INIT;
      const granit_render_pipeline_environment* environment_ptr = nullptr;
      if (desc.environment) {
        environment = to_native(*desc.environment);
        environment_ptr = &environment;
      }
      const granit_render_pipeline_render_desc native{
          .struct_size = GRANIT_RENDER_PIPELINE_RENDER_DESC_VERSION_1_SIZE,
          .reserved = 0,
          .scene = desc.scene.native_handle(),
          .output = desc.output.native_handle(),
          .output_format = static_cast<granit_texture_format>(desc.output_format),
          .width = desc.width,
          .height = desc.height,
          .first_view = desc.first_view,
          .view_count = desc.view_count,
          .exposure_ev = desc.exposure_ev,
          .draw_binding_count = static_cast<std::uint32_t>(bindings.size()),
          .draw_bindings = bindings.data(),
          .output_count = static_cast<std::uint32_t>(outputs.size()),
          .outputs = outputs.data(),
          .frame = desc.frame ? desc.frame->handle : GRANIT_NULL_HANDLE,
          .reserved_tail = 0,
          .canvas = desc.canvas.native_handle(),
          .debug_draw = desc.debug_draw.native_handle(),
          .clear_color = {desc.clear_color.red, desc.clear_color.green, desc.clear_color.blue,
                          desc.clear_color.alpha},
          .environment = environment_ptr,
      };
      return render(native);
    } catch (const std::bad_alloc&) {
      return result::out_of_memory;
    } catch (...) {
      return result::internal;
    }
  }
  [[nodiscard]] result enable_metrics() noexcept {
    return from_native(granit_render_pipeline_metrics_enable(renderer_, handle_));
  }
  [[nodiscard]] result get_metrics(granit_render_pipeline_metrics& metrics) const noexcept {
    return from_native(granit_render_pipeline_get_metrics(renderer_, handle_, &metrics));
  }
  [[nodiscard]] result get_metrics(render_pipeline_metrics& metrics) const noexcept {
    granit_render_pipeline_metrics native = GRANIT_RENDER_PIPELINE_METRICS_INIT;
    const auto value = get_metrics(native);
    if (value.ok()) {
      metrics = {.sample_sequence = native.sample_sequence,
                 .shadow_gpu_ns = native.shadow_gpu_ns,
                 .opaque_gpu_ns = native.opaque_gpu_ns,
                 .tone_mapping_gpu_ns = native.tone_mapping_gpu_ns,
                 .total_gpu_ns = native.total_gpu_ns};
    }
    return value;
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    return from_native(granit_render_pipeline_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_render_pipeline native_handle() const noexcept { return handle_; }

private:
  [[nodiscard]] static constexpr granit_render_pipeline_output
  to_native(const render_pipeline_output& output) noexcept {
    return {.struct_size = GRANIT_RENDER_PIPELINE_OUTPUT_VERSION_1_SIZE,
            .reserved = 0,
            .view = output.view.native_handle(),
            .format = static_cast<granit_texture_format>(output.format),
            .width = output.width,
            .height = output.height,
            .reserved_tail = 0,
            .canvas = output.canvas.native_handle(),
            .debug_draw = output.debug_draw.native_handle()};
  }

  [[nodiscard]] static constexpr granit_render_pipeline_environment
  to_native(const render_pipeline_environment& environment) noexcept {
    return {.struct_size = GRANIT_RENDER_PIPELINE_ENVIRONMENT_VERSION_1_SIZE,
            .reserved = 0,
            .irradiance = environment.irradiance.native_handle(),
            .prefiltered_environment = environment.prefiltered_environment.native_handle(),
            .brdf_lut = environment.brdf_lut.native_handle(),
            .rotation_radians = environment.rotation_radians,
            .intensity = environment.intensity,
            .prefiltered_max_mip = environment.prefiltered_max_mip,
            .reserved_tail = 0};
  }

  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_render_pipeline handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
