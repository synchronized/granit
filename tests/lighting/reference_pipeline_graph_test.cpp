// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/reference_pipeline_graph.h"

#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <string>
#include <vector>

namespace {

granit_texture_desc texture_desc(granit_texture_format format, granit_texture_usage usage) {
  granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
  desc.format = format;
  desc.usage = usage;
  desc.width = 16;
  desc.height = 16;
  return desc;
}

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("参考管线Graph按Shadow PBR Tone Mapping排序") {
  granit::render_graph::serial_graph graph;
  const auto shadow = graph.create_transient_texture(
      texture_desc(GRANIT_TEXTURE_FORMAT_D32_FLOAT,
                   GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       GRANIT_TEXTURE_USAGE_SAMPLED_BIT),
      "Shadow");
  const auto hdr = graph.create_transient_texture(
      texture_desc(GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
                   GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                       GRANIT_TEXTURE_USAGE_SAMPLED_BIT),
      "HDR");
  const auto depth = graph.create_transient_texture(
      texture_desc(GRANIT_TEXTURE_FORMAT_D32_FLOAT,
                   GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
      "Depth");
  const auto output = graph.create_transient_texture(
      texture_desc(GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                   GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT),
      "Output");

  granit::lighting::reference_pipeline_graph_desc desc;
  granit::lighting::directional_shadow_pass_desc shadow_desc;
  shadow_desc.depth = shadow;
  shadow_desc.casters.push_back({});
  desc.shadow = std::move(shadow_desc);
  desc.pbr.color = hdr;
  desc.pbr.depth = depth;
  desc.pbr.shadow = shadow;
  desc.pbr.view.view_projection = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  desc.pbr.light.direction_to_light = {0, 0, 1};
  desc.pbr.objects.push_back({
      .model = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
      .normal_matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}});
  desc.tone_mapping = {
      .hdr_color = hdr,
      .output = output,
      .output_format = granit::texture_format::rgba8_unorm,
      .tone_mapping = {
          .output_transfer = granit::lighting::tone_mapping_output_transfer::shader_srgb}};
  std::vector<std::string> recorded;
  granit::lighting::reference_pipeline_graph_callbacks callbacks{
      .shadow = [&](auto&, const auto&, auto) {
        recorded.emplace_back("shadow");
        return GRANIT_SUCCESS;
      },
      .pbr = [&](auto&, const auto&, auto) {
        recorded.emplace_back("pbr");
        return GRANIT_SUCCESS;
      },
      .tone_mapping = [&](auto&, const auto&) {
        recorded.emplace_back("tone");
        return GRANIT_SUCCESS;
      }};
  granit::lighting::reference_pipeline_graph_passes passes;
  REQUIRE(granit::lighting::add_reference_pipeline_graph(graph, std::move(desc),
                                                         std::move(callbacks), passes) ==
          granit::lighting::reference_pipeline_graph_error::none);
  CHECK(passes.shadow != granit::render_graph::invalid_pass_id);
  CHECK(passes.pbr != granit::render_graph::invalid_pass_id);
  CHECK(passes.tone_mapping != granit::render_graph::invalid_pass_id);

  const auto diagnostics = graph.diagnostics();
  REQUIRE(diagnostics.compilation.succeeded());
  REQUIRE(diagnostics.compilation.execution_order.size() == 3);
  CHECK(diagnostics.pass_names[diagnostics.compilation.execution_order[0]].ends_with("Shadow"));
  CHECK(diagnostics.pass_names[diagnostics.compilation.execution_order[1]].ends_with("PBR HDR"));
  CHECK(
      diagnostics.pass_names[diagnostics.compilation.execution_order[2]].ends_with("Tone Mapping"));

  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-reference-graph"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  for (std::uint32_t frame = 0; frame < 2; ++frame) {
    const auto result = graph.execute(renderer.native_handle());
    REQUIRE(result.succeeded());
    REQUIRE(granit_command_recorder_destroy(renderer.native_handle(), result.recorder) ==
            GRANIT_SUCCESS);
  }
  const std::array expected{"shadow", "pbr", "tone", "shadow", "pbr", "tone"};
  CHECK(recorded == std::vector<std::string>{expected.begin(), expected.end()});
}

TEST_CASE("参考管线Graph拒绝不一致资源且不修改输出") {
  granit::render_graph::serial_graph graph;
  const auto first = graph.create_transient_texture(
      texture_desc(GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
                   GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT),
      "First");
  const auto second = graph.create_transient_texture(
      texture_desc(GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT, GRANIT_TEXTURE_USAGE_SAMPLED_BIT),
      "Second");
  const auto depth = graph.create_transient_texture(
      texture_desc(GRANIT_TEXTURE_FORMAT_D32_FLOAT,
                   GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
      "Depth");
  const auto output = graph.create_transient_texture(
      texture_desc(GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                   GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT),
      "Output");
  granit::lighting::reference_pipeline_graph_passes passes{.pbr = 42};
  granit::lighting::reference_pipeline_graph_callbacks callbacks;
  callbacks.pbr = [](auto&, const auto&, auto) { return GRANIT_SUCCESS; };
  callbacks.tone_mapping = [](auto&, const auto&) { return GRANIT_SUCCESS; };
  granit::lighting::reference_pipeline_graph_desc desc;
  desc.pbr.color = first;
  desc.pbr.depth = depth;
  desc.tone_mapping.hdr_color = second;
  desc.tone_mapping.output = output;
  desc.tone_mapping.output_format = granit::texture_format::rgba8_unorm;
  desc.tone_mapping.tone_mapping.output_transfer =
      granit::lighting::tone_mapping_output_transfer::shader_srgb;
  CHECK(granit::lighting::add_reference_pipeline_graph(
            graph, std::move(desc), std::move(callbacks), passes) ==
        granit::lighting::reference_pipeline_graph_error::inconsistent_resource);
  CHECK(passes.pbr == 42);
  CHECK(graph.diagnostics().compilation.execution_order.empty());
}
