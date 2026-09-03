// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/tone_mapping_pass.h"

#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("HDR Attachment使用RGBA16_FLOAT并支持采样") {
  const auto desc = granit::lighting::make_hdr_attachment_desc(1280, 720);
  CHECK(desc.format == GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT);
  CHECK(desc.width == 1280);
  CHECK(desc.height == 720);
  CHECK((desc.usage & GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) != 0);
  CHECK((desc.usage & GRANIT_TEXTURE_USAGE_SAMPLED_BIT) != 0);
}

TEST_CASE("Tone Mapping Pass声明HDR读和最终目标写") {
  granit::render_graph::serial_graph graph;
  const auto hdr =
      graph.create_transient_texture(granit::lighting::make_hdr_attachment_desc(4, 4), "HDR Color");
  const auto output = graph.import_texture_view(101, true, "Output");
  REQUIRE(graph.add_pass(
              {.accesses = {{hdr, granit::render_graph::access_type::write}}},
              [](granit::render_graph::pass_context&) { return GRANIT_SUCCESS; },
              "HDR Producer") != granit::render_graph::invalid_pass_id);
  bool called = false;
  const auto pass = granit::lighting::add_tone_mapping_graph_pass(
      graph,
      {.hdr_color = hdr,
       .output = output,
       .output_format = granit::texture_format::rgba8_unorm,
       .tone_mapping = {.exposure_ev = 1.0F,
                        .output_transfer =
                            granit::lighting::tone_mapping_output_transfer::shader_srgb,
                        .enable_fxaa = false}},
      [&](granit::render_graph::pass_context& context,
          const granit::lighting::tone_mapping_constants& constants) {
        called = true;
        CHECK(context.texture_view(hdr) != GRANIT_NULL_HANDLE);
        CHECK(context.texture_view(output) == 101);
        CHECK(constants.exposure_scale == 2.0F);
        CHECK(constants.encode_srgb == 1);
        CHECK(constants.enable_fxaa == 0);
        return GRANIT_SUCCESS;
      });
  REQUIRE(pass != granit::render_graph::invalid_pass_id);

  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-tone-mapping-pass"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  const auto result = graph.execute(renderer.native_handle());
  REQUIRE(result.succeeded());
  CHECK(called);
  REQUIRE(granit_command_recorder_destroy(renderer.native_handle(), result.recorder) ==
          GRANIT_SUCCESS);
}

TEST_CASE("Tone Mapping Pass拒绝格式编码误配和资源别名") {
  granit::render_graph::serial_graph graph;
  const auto resource = graph.import_texture_view(101, true);
  const auto callback = [](granit::render_graph::pass_context&,
                           const granit::lighting::tone_mapping_constants&) {
    return GRANIT_SUCCESS;
  };
  CHECK(granit::lighting::add_tone_mapping_graph_pass(
            graph,
            {.hdr_color = resource,
             .output = resource,
             .output_format = granit::texture_format::rgba8_unorm,
             .tone_mapping = {.output_transfer =
                                  granit::lighting::tone_mapping_output_transfer::shader_srgb}},
            callback) == granit::render_graph::invalid_pass_id);
  CHECK(granit::lighting::add_tone_mapping_graph_pass(
            graph,
            {.hdr_color = resource,
             .output = graph.import_texture_view(102, true),
             .output_format = granit::texture_format::rgba8_srgb,
             .tone_mapping = {.output_transfer =
                                  granit::lighting::tone_mapping_output_transfer::shader_srgb}},
            callback) == granit::render_graph::invalid_pass_id);
}
