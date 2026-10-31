// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/render_graph/render_graph.hpp>
#include <granit/renderer/renderer.hpp>

#include <cstdint>
#include <cstdio>
#include <span>

namespace {

struct pass_state {
  granit::render_graph::resource_id output = granit::render_graph::invalid_resource;
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

granit::render_graph::access_desc access(granit::render_graph::resource_id resource,
                                         granit::render_graph::access direction,
                                         granit::render_graph::usage kind) {
  return {resource, direction, kind};
}

} // namespace

int main() {
  granit::render_graph::graph graph;
  if (graph.initialize().failed())
    return 1;

  granit::renderer renderer;
  if (renderer.initialize({.application_name = "Granit Render Graph Example"}).failed())
    return 1;

  granit::texture_desc texture_desc;
  texture_desc.width = 1280;
  texture_desc.height = 720;
  texture_desc.depth = 1;
  texture_desc.mip_levels = 1;
  texture_desc.array_layers = 1;
  texture_desc.format = granit::texture_format::rgba16_float;
  texture_desc.usage = granit::texture_usage::sampled | granit::texture_usage::color_attachment;

  granit::render_graph::resource_id shadow = granit::render_graph::invalid_resource;
  granit::render_graph::resource_id hdr = granit::render_graph::invalid_resource;
  granit::render_graph::resource_id output = granit::render_graph::invalid_resource;
  if (graph.create_transient_texture(texture_desc, "shadow", shadow).failed() ||
      graph.create_transient_texture(texture_desc, "hdr", hdr).failed() ||
      graph.create_transient_texture(texture_desc, "output", output).failed())
    return 1;

  const granit::render_graph::access_desc shadow_write = access(
      shadow, granit::render_graph::access::write, granit::render_graph::usage::color_attachment);
  granit::render_graph::pass_id shadow_pass = granit::render_graph::invalid_pass;
  granit::render_graph::pass_desc shadow_desc{.name = "Shadow",
                                              .accesses = std::span{&shadow_write, 1}};
  pass_state shadow_state{shadow, texture_desc.width, texture_desc.height};
  if (graph.add_pass(shadow_desc, record_pass, &shadow_state, shadow_pass).failed())
    return 1;

  const granit::render_graph::access_desc scene_accesses[] = {
      access(shadow, granit::render_graph::access::read, granit::render_graph::usage::sampled),
      access(hdr, granit::render_graph::access::write,
             granit::render_graph::usage::color_attachment),
  };
  granit::render_graph::pass_id scene_pass = granit::render_graph::invalid_pass;
  granit::render_graph::pass_desc scene_desc{.name = "Scene", .accesses = scene_accesses};
  pass_state scene_state{hdr, texture_desc.width, texture_desc.height};
  if (graph.add_pass(scene_desc, record_pass, &scene_state, scene_pass).failed())
    return 1;

  const granit::render_graph::access_desc tone_accesses[] = {
      access(hdr, granit::render_graph::access::read, granit::render_graph::usage::sampled),
      access(output, granit::render_graph::access::write,
             granit::render_graph::usage::color_attachment),
  };
  granit::render_graph::pass_desc tone_desc{
      .side_effect = true, .name = "Tone Mapping", .accesses = tone_accesses};
  granit::render_graph::pass_id tone_pass = granit::render_graph::invalid_pass;
  pass_state tone_state{output, texture_desc.width, texture_desc.height};
  if (graph.add_pass(tone_desc, record_pass, &tone_state, tone_pass).failed())
    return 1;

  if (graph.add_dependency(shadow_pass, scene_pass).failed() ||
      graph.add_dependency(scene_pass, tone_pass).failed() || graph.compile().failed())
    return 1;

  granit::render_graph::execution_result execution;
  if (graph.execute(renderer.native_handle(), GRANIT_NULL_HANDLE, UINT32_MAX, execution).failed())
    return 1;
  if (granit_command_recorder_destroy(renderer.native_handle(), execution.recorder) !=
      GRANIT_SUCCESS)
    return 1;

  std::puts("Shadow -> Scene -> Tone Mapping graph executed");
  return 0;
}
