// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_UNLIT_PASS_H_
#define GRANIT_PIPELINE_UNLIT_PASS_H_

#include "material/pbr_draw_inputs.h"

#include <granit/pipeline/export.h>
#include <granit/pipeline/material.h>
#include <granit/pipeline/mesh.h>
#include <granit/renderer/render_target.h>

namespace granit::pipeline::detail {

enum class unlit_mode : uint8_t { opaque, alpha_cutoff, transparent };

struct unlit_pass_desc {
  granit_texture_view color = GRANIT_NULL_HANDLE;
  granit_texture_view depth = GRANIT_NULL_HANDLE;
  granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  granit_texture_format depth_format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  uint32_t width = 0;
  uint32_t height = 0;
  granit_mesh mesh = GRANIT_NULL_HANDLE;
  granit_material material = GRANIT_NULL_HANDLE;
  granit::material::pbr_frame_constants frame{};
  granit::material::pbr_object_constants object{};
  unlit_mode mode = unlit_mode::opaque;
  granit_attachment_load_operation color_load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR;
  granit_scissor scissor{};
};

/** 在现有 Recorder 中录制一次 Unlit Draw；资源均由调用方持有。 */
[[nodiscard]] GRANIT_RENDER_PIPELINE_API granit_result
record_unlit_pass(granit_renderer renderer, granit_command_recorder recorder,
                  const unlit_pass_desc& desc) noexcept;

} // namespace granit::pipeline::detail

#endif
