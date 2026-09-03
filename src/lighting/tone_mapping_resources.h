// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_TONE_MAPPING_RESOURCES_H
#define GRANIT_LIGHTING_TONE_MAPPING_RESOURCES_H

#include "lighting/tone_mapping_pass.h"

#include <granit/renderer/buffer.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/texture.h>

namespace granit::lighting {

/** 可跨帧复用的 Tone Mapping Shader、布局、Sampler 和全屏 Pipeline。 */
class tone_mapping_pipeline_resources {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer,
                                         granit::texture_format output_format,
                                         std::span<const std::byte> vertex_shader,
                                         std::span<const std::byte> fragment_shader,
                                         std::string_view wgsl = {}) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] bool initialized() const noexcept { return pipeline_.valid(); }
  [[nodiscard]] granit_graphics_pipeline pipeline() const noexcept {
    return pipeline_.native_handle();
  }
  [[nodiscard]] granit_pipeline_layout pipeline_layout() const noexcept {
    return pipeline_layout_.native_handle();
  }
  [[nodiscard]] granit_bind_group_layout group_layout() const noexcept {
    return group_layout_.native_handle();
  }
  [[nodiscard]] granit_sampler sampler() const noexcept { return sampler_.native_handle(); }
  [[nodiscard]] granit_renderer renderer() const noexcept { return renderer_; }
  [[nodiscard]] granit::texture_format output_format() const noexcept { return output_format_; }

private:
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit::texture_format output_format_ = granit::texture_format::undefined;
  granit::sampler sampler_;
  granit::bind_group_layout group_layout_;
  granit::pipeline_layout pipeline_layout_;
  granit::shader vertex_shader_;
  granit::shader fragment_shader_;
  granit::graphics_pipeline pipeline_;
};

/** 单个 HDR View 对应的常量 Buffer 与 Bind Group，可独立逐帧重建。 */
class tone_mapping_binding_resources {
public:
  [[nodiscard]] granit_result initialize(const tone_mapping_pipeline_resources& pipeline,
                                         granit_texture_view hdr_view,
                                         const tone_mapping_constants& constants) noexcept;
  [[nodiscard]] granit_result update(const tone_mapping_constants& constants) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] bool initialized() const noexcept { return group_.valid(); }
  [[nodiscard]] granit_bind_group group() const noexcept { return group_.native_handle(); }

private:
  granit::buffer constants_;
  granit::bind_group group_;
};

/** 兼容单次初始化用法的组合资源；新管线应分别缓存 pipeline 并按 HDR View 创建 binding。 */
class tone_mapping_resources {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer, granit_texture_view hdr_view,
                                         granit::texture_format output_format,
                                         const tone_mapping_constants& constants,
                                         std::span<const std::byte> vertex_shader,
                                         std::span<const std::byte> fragment_shader) noexcept;
  [[nodiscard]] granit_result update(const tone_mapping_constants& constants) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] bool initialized() const noexcept { return pipeline_.initialized(); }
  [[nodiscard]] granit_graphics_pipeline pipeline() const noexcept { return pipeline_.pipeline(); }
  [[nodiscard]] granit_pipeline_layout pipeline_layout() const noexcept {
    return pipeline_.pipeline_layout();
  }
  [[nodiscard]] granit_bind_group group() const noexcept { return binding_.group(); }

private:
  tone_mapping_pipeline_resources pipeline_;
  tone_mapping_binding_resources binding_;
};

} // namespace granit::lighting

#endif
