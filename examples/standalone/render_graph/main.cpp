// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/render_graph.hpp>

#include <cstdio>

namespace {

granit_result record_pass(const granit_render_graph_pass_context*, void*) {
  return GRANIT_SUCCESS;
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
  if (granit_render_graph_create_transient_texture(graph.native_handle(), &texture_desc, "shadow",
                                                   6, &shadow) != GRANIT_SUCCESS ||
      granit_render_graph_create_transient_texture(graph.native_handle(), &texture_desc, "hdr", 3,
                                                   &hdr) != GRANIT_SUCCESS ||
      granit_render_graph_create_transient_texture(graph.native_handle(), &texture_desc, "output",
                                                   6, &output) != GRANIT_SUCCESS)
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
  if (granit_render_graph_add_pass(graph.native_handle(), &shadow_desc, record_pass, nullptr,
                                   &shadow_pass) != GRANIT_SUCCESS)
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
  if (granit_render_graph_add_pass(graph.native_handle(), &scene_desc, record_pass, nullptr,
                                   &scene_pass) != GRANIT_SUCCESS)
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
  if (granit_render_graph_add_pass(graph.native_handle(), &tone_desc, record_pass, nullptr,
                                   &tone_pass) != GRANIT_SUCCESS)
    return 1;

  if (granit_render_graph_add_dependency(graph.native_handle(), shadow_pass, scene_pass) !=
          GRANIT_SUCCESS ||
      granit_render_graph_add_dependency(graph.native_handle(), scene_pass, tone_pass) !=
          GRANIT_SUCCESS ||
      granit_render_graph_compile(graph.native_handle()) != GRANIT_SUCCESS)
    return 1;

  std::puts("Shadow -> Scene -> Tone Mapping graph compiled");
  return 0;
}
