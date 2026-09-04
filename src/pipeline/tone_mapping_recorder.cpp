// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/tone_mapping_recorder.h"

#include "pipeline/embedded_shaders.h"

#include <granit/renderer/command_recorder.h>
#include <granit/renderer/render_target.h>

namespace granit::pipeline::detail {

granit_result record_tone_mapping(lighting::tone_mapping_pipeline_resources& pipeline,
                                  granit_renderer renderer, granit_command_recorder recorder,
                                  granit_texture_view hdr_view, granit_texture_view output_view,
                                  granit_texture_format output_format, std::uint32_t width,
                                  std::uint32_t height,
                                  const lighting::tone_mapping_constants& constants) {
  auto sampling_constants = constants;
  sampling_constants.inverse_width = 1.0F / static_cast<float>(width);
  sampling_constants.inverse_height = 1.0F / static_cast<float>(height);
  if (!pipeline.initialized()) {
    const auto initialize = pipeline.initialize(
        renderer, static_cast<granit::texture_format>(output_format), tone_mapping_vertex_shader(),
        tone_mapping_fragment_shader(), tone_mapping_wgsl());
    if (initialize != GRANIT_SUCCESS)
      return initialize;
  }
  lighting::tone_mapping_binding_resources binding;
  auto result = binding.initialize(pipeline, hdr_view, sampling_constants);
  if (result == GRANIT_SUCCESS)
    result =
        granit_command_recorder_bind_graphics_pipeline(renderer, recorder, pipeline.pipeline());
  const auto group = binding.group();
  if (result == GRANIT_SUCCESS) {
    const granit_bind_groups_desc bind_desc{
        GRANIT_BIND_GROUPS_DESC_VERSION_1_SIZE, 0, &group, 1, 0, nullptr};
    result = granit_command_recorder_bind_graphics_groups(renderer, recorder,
                                                          pipeline.pipeline_layout(), &bind_desc);
  }
  const granit_viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
  const granit_scissor scissor{0, 0, width, height};
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_set_viewports(renderer, recorder, 0, &viewport, 1);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_set_scissors(renderer, recorder, 0, &scissor, 1);
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = output_view;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.area = {0, 0, width, height};
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_begin_rendering(renderer, recorder, &rendering);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_draw(renderer, recorder, 3, 1, 0, 0);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end_rendering(renderer, recorder);
  const auto reset_result = binding.reset();
  return result == GRANIT_SUCCESS ? reset_result : result;
}

} // namespace granit::pipeline::detail
