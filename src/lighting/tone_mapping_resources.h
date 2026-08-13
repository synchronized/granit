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

/** 拥有单个 HDR View 对应的 Tone Mapping Bind Group、Shader 和全屏 Pipeline。 */
class tone_mapping_resources {
public:
  [[nodiscard]] granit_result initialize(
      granit_renderer renderer, granit_texture_view hdr_view, granit::texture_format output_format,
      const tone_mapping_constants& constants, std::span<const std::byte> vertex_shader,
      std::span<const std::byte> fragment_shader) noexcept;
  [[nodiscard]] granit_result update(const tone_mapping_constants& constants) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] bool initialized() const noexcept { return pipeline_.valid(); }
  [[nodiscard]] granit_graphics_pipeline pipeline() const noexcept {
    return pipeline_.native_handle();
  }
  [[nodiscard]] granit_pipeline_layout pipeline_layout() const noexcept {
    return pipeline_layout_.native_handle();
  }
  [[nodiscard]] granit_bind_group group() const noexcept { return group_.native_handle(); }

private:
  granit::buffer constants_;
  granit::sampler sampler_;
  granit::bind_group_layout group_layout_;
  granit::bind_group group_;
  granit::pipeline_layout pipeline_layout_;
  granit::shader vertex_shader_;
  granit::shader fragment_shader_;
  granit::graphics_pipeline pipeline_;
};

} // namespace granit::lighting

#endif
