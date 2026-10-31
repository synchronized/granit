// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/render_graph.hpp>
#include <granit/renderer/renderer.hpp>

#include <cstdio>
#include <cstdint>

namespace {

struct pass_state {
  granit_render_graph_resource output = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

granit_result record_pass(const granit_render_graph_pass_context* context, void* user_data) {
  const auto& state = *static_cast<const pass_state*>(user_data);
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = granit_render_graph_context_texture_view(context, state.output);
  if (color.view == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachments = &color;
  rendering.color_attachment_count = 1;
  rendering.area.width = state.width;
  rendering.area.height = state.height;
  auto result = granit_command_recorder_begin_rendering(
      granit_render_graph_context_renderer(context), granit_render_graph_context_recorder(context),
      &rendering);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end_rendering(granit_render_graph_context_renderer(context),
                                                   granit_render_graph_context_recorder(context));
  return result;
}

granit_render_graph_access_desc access(granit_render_graph_resource resource,
                                       granit_render_graph_access direction,
                                       granit_render_graph_usage usage) {
  return {GRANIT_RENDER_GRAPH_ACCESS_DESC_VERSION_1_SIZE, resource, direction, usage, 0};
}

} // namespace

int main() {
  granit::render_graph_handle graph;
  if (graph.initialize().failed())
    return 1;

  granit::renderer renderer;
  if (renderer.initialize({.application_name = "Granit Render Graph Example"}).failed())
    return 1;

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.width = 1280;
  texture_desc.height = 720;
  texture_desc.depth = 1;
  texture_desc.mip_levels = 1;
  texture_desc.array_layers = 1;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT |
                       GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

  granit_render_graph_resource shadow = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
  granit_render_graph_resource hdr = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
  granit_render_graph_resource output = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
  if (graph.create_transient_texture(texture_desc, "shadow", shadow).failed() ||
      graph.create_transient_texture(texture_desc, "hdr", hdr).failed() ||
      graph.create_transient_texture(texture_desc, "output", output).failed())
    return 1;

  const granit_render_graph_access_desc shadow_write =
      access(shadow, GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT,
             GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT);
  granit_render_graph_pass shadow_pass = GRANIT_RENDER_GRAPH_INVALID_PASS;
  granit_render_graph_pass_desc shadow_desc = GRANIT_RENDER_GRAPH_PASS_DESC_INIT;
  shadow_desc.name = "Shadow";
  shadow_desc.name_length = 6;
  shadow_desc.accesses = &shadow_write;
  shadow_desc.access_count = 1;
  pass_state shadow_state{shadow, texture_desc.width, texture_desc.height};
  if (graph.add_pass(shadow_desc, record_pass, &shadow_state, shadow_pass).failed())
    return 1;

  const granit_render_graph_access_desc scene_accesses[] = {
      access(shadow, GRANIT_RENDER_GRAPH_ACCESS_READ_BIT, GRANIT_RENDER_GRAPH_USAGE_SAMPLED_BIT),
      access(hdr, GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT,
             GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT),
  };
  granit_render_graph_pass scene_pass = GRANIT_RENDER_GRAPH_INVALID_PASS;
  granit_render_graph_pass_desc scene_desc = GRANIT_RENDER_GRAPH_PASS_DESC_INIT;
  scene_desc.name = "Scene";
  scene_desc.name_length = 5;
  scene_desc.accesses = scene_accesses;
  scene_desc.access_count = 2;
  pass_state scene_state{hdr, texture_desc.width, texture_desc.height};
  if (graph.add_pass(scene_desc, record_pass, &scene_state, scene_pass).failed())
    return 1;

  const granit_render_graph_access_desc tone_accesses[] = {
      access(hdr, GRANIT_RENDER_GRAPH_ACCESS_READ_BIT, GRANIT_RENDER_GRAPH_USAGE_SAMPLED_BIT),
      access(output, GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT,
             GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT),
  };
  granit_render_graph_pass_desc tone_desc = GRANIT_RENDER_GRAPH_PASS_DESC_INIT;
  tone_desc.flags = GRANIT_RENDER_GRAPH_PASS_SIDE_EFFECT_BIT;
  tone_desc.name = "Tone Mapping";
  tone_desc.name_length = 11;
  tone_desc.accesses = tone_accesses;
  tone_desc.access_count = 2;
  granit_render_graph_pass tone_pass = GRANIT_RENDER_GRAPH_INVALID_PASS;
  pass_state tone_state{output, texture_desc.width, texture_desc.height};
  if (graph.add_pass(tone_desc, record_pass, &tone_state, tone_pass).failed())
    return 1;

  if (graph.add_dependency(shadow_pass, scene_pass).failed() ||
      graph.add_dependency(scene_pass, tone_pass).failed() || graph.compile().failed())
    return 1;

  granit_render_graph_execution_result execution = GRANIT_RENDER_GRAPH_EXECUTION_RESULT_INIT;
  if (graph.execute(renderer.native_handle(), GRANIT_NULL_HANDLE, UINT32_MAX, execution).failed())
    return 1;
  if (granit_command_recorder_destroy(renderer.native_handle(), execution.recorder) != GRANIT_SUCCESS)
    return 1;

  std::puts("Shadow -> Scene -> Tone Mapping graph executed");
  return 0;
}
