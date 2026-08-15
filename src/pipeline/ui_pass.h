// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_UI_PASS_H_
#define GRANIT_PIPELINE_UI_PASS_H_

#include "material/pbr_draw_inputs.h"
#include "pipeline/canvas_draw_list.h"
#include "pipeline/ui_geometry_upload.h"

#include <granit/pipeline/export.h>
#include <granit/pipeline/material.h>
#include <granit/renderer/render_target.h>

namespace granit::pipeline::detail {

struct ui_pass_desc {
  granit_texture_view color = GRANIT_NULL_HANDLE;
  granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  granit_material material = GRANIT_NULL_HANDLE;
  granit::material::pbr_frame_constants frame{};
  granit::material::pbr_object_constants object{};
  granit_attachment_load_operation load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD;
};

/** 在一个 Rendering 区域内按稳定顺序录制带 Texture/Sampler 的 UI Batch。 */
[[nodiscard]] GRANIT_RENDER_PIPELINE_API granit_result
record_ui_pass(granit_renderer renderer, granit_command_recorder recorder, const ui_pass_desc& desc,
               const canvas_draw_list& list, const ui_geometry_upload& geometry) noexcept;

} // namespace granit::pipeline::detail

#endif
