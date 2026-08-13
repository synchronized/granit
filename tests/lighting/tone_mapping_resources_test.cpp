// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/tone_mapping_reference.h"
#include "lighting/tone_mapping_resources.h"

#include <granit/granit.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> load_shader(const char* name) {
  std::ifstream stream{std::string{GRANIT_LIGHTING_ASSET_DIR} + "/" + name, std::ios::binary};
  const std::vector<char> source{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> result(source.size());
  if (!source.empty())
    std::memcpy(result.data(), source.data(), source.size());
  return result;
}

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("Tone Mapping GPU资源建立完整全屏Pipeline") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-tone-mapping-gpu"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::texture hdr_texture;
  granit::texture_view hdr_view;
  REQUIRE(hdr_texture.initialize(renderer.native_handle(),
                                 {.format = granit::texture_format::rgba16_float,
                                  .usage = granit::texture_usage::sampled}) ==
          granit::result::success);
  REQUIRE(hdr_view.initialize(renderer.native_handle(), hdr_texture.native_handle()) ==
          granit::result::success);
  const auto vertex = load_shader("tone_mapping.vert.spv");
  const auto fragment = load_shader("tone_mapping.frag.spv");
  REQUIRE_FALSE(vertex.empty());
  REQUIRE_FALSE(fragment.empty());

  granit::lighting::tone_mapping_resources resources;
  REQUIRE(resources.initialize(renderer.native_handle(), hdr_view.native_handle(),
                               granit::texture_format::rgba8_unorm,
                               {.exposure_scale = 2.0F, .encode_srgb = 1}, vertex,
                               fragment) == GRANIT_SUCCESS);
  CHECK(resources.pipeline() != GRANIT_NULL_HANDLE);
  CHECK(resources.pipeline_layout() != GRANIT_NULL_HANDLE);
  CHECK(resources.group() != GRANIT_NULL_HANDLE);
  CHECK(resources.update({.exposure_scale = 0.5F, .encode_srgb = 0}) == GRANIT_SUCCESS);
  CHECK(resources.update({.exposure_scale = 0.0F}) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Tone Mapping跨HDR View复用不变Pipeline资源") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-tone-cache"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  std::array<granit::texture, 2> textures;
  std::array<granit::texture_view, 2> views;
  for (std::size_t index = 0; index < views.size(); ++index) {
    REQUIRE(textures[index].initialize(renderer.native_handle(),
                                       {.format = granit::texture_format::rgba16_float,
                                        .usage = granit::texture_usage::sampled}) ==
            granit::result::success);
    REQUIRE(views[index].initialize(renderer.native_handle(), textures[index].native_handle()) ==
            granit::result::success);
  }
  granit::lighting::tone_mapping_pipeline_resources pipeline;
  REQUIRE(pipeline.initialize(renderer.native_handle(), granit::texture_format::rgba8_unorm,
                              load_shader("tone_mapping.vert.spv"),
                              load_shader("tone_mapping.frag.spv")) == GRANIT_SUCCESS);
  const auto pipeline_handle = pipeline.pipeline();
  REQUIRE(pipeline_handle != GRANIT_NULL_HANDLE);
  for (auto& view : views) {
    granit::lighting::tone_mapping_binding_resources binding;
    REQUIRE(binding.initialize(pipeline, view.native_handle(),
                               {.exposure_scale = 1.0F, .encode_srgb = 1}) == GRANIT_SUCCESS);
    CHECK(binding.group() != GRANIT_NULL_HANDLE);
    CHECK(pipeline.pipeline() == pipeline_handle);
  }
}

TEST_CASE("Tone Mapping GPU资源拒绝不完整输入") {
  granit::lighting::tone_mapping_resources resources;
  CHECK(resources.initialize(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE,
                             granit::texture_format::undefined, {}, {},
                             {}) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(resources.update({}) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Tone Mapping GPU资源拒绝重复或缺失sRGB编码") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-tone-transfer"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  granit::texture hdr_texture;
  granit::texture_view hdr_view;
  REQUIRE(hdr_texture.initialize(renderer.native_handle(),
                                 {.format = granit::texture_format::rgba16_float,
                                  .usage = granit::texture_usage::sampled}) ==
          granit::result::success);
  REQUIRE(hdr_view.initialize(renderer.native_handle(), hdr_texture.native_handle()) ==
          granit::result::success);
  const auto vertex = load_shader("tone_mapping.vert.spv");
  const auto fragment = load_shader("tone_mapping.frag.spv");

  granit::lighting::tone_mapping_resources missing_encoding;
  CHECK(missing_encoding.initialize(renderer.native_handle(), hdr_view.native_handle(),
                                    granit::texture_format::rgba8_unorm,
                                    {.exposure_scale = 1.0F, .encode_srgb = 0}, vertex,
                                    fragment) == GRANIT_ERROR_INVALID_ARGUMENT);
  granit::lighting::tone_mapping_resources duplicate_encoding;
  CHECK(duplicate_encoding.initialize(renderer.native_handle(), hdr_view.native_handle(),
                                      granit::texture_format::rgba8_srgb,
                                      {.exposure_scale = 1.0F, .encode_srgb = 1}, vertex,
                                      fragment) == GRANIT_ERROR_INVALID_ARGUMENT);
  granit::lighting::tone_mapping_resources attachment_encoding;
  REQUIRE(attachment_encoding.initialize(renderer.native_handle(), hdr_view.native_handle(),
                                         granit::texture_format::rgba8_srgb,
                                         {.exposure_scale = 1.0F, .encode_srgb = 0}, vertex,
                                         fragment) == GRANIT_SUCCESS);
}

TEST_CASE("Tone Mapping GPU输出与CPU参考一致") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-tone-mapping-pixel"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::texture hdr_texture;
  granit::texture_view hdr_view;
  REQUIRE(hdr_texture.initialize(renderer.native_handle(),
                                 {.format = granit::texture_format::rgba16_float,
                                  .usage = granit::texture_usage::sampled |
                                           granit::texture_usage::transfer_destination}) ==
          granit::result::success);
  constexpr std::array<std::uint16_t, 4> hdr_pixel{0x4400, 0x3c00, 0x3400, 0x3c00};
  REQUIRE(
      hdr_texture.write({reinterpret_cast<const std::byte*>(hdr_pixel.data()), sizeof(hdr_pixel)},
                        {.bytes_per_row = 8}, {}) == granit::result::success);
  REQUIRE(hdr_view.initialize(renderer.native_handle(), hdr_texture.native_handle()) ==
          granit::result::success);

  granit::lighting::tone_mapping_resources resources;
  REQUIRE(resources.initialize(renderer.native_handle(), hdr_view.native_handle(),
                               granit::texture_format::rgba8_unorm,
                               {.exposure_scale = 2.0F, .encode_srgb = 1},
                               load_shader("tone_mapping.vert.spv"),
                               load_shader("tone_mapping.frag.spv")) == GRANIT_SUCCESS);
  granit_texture output_texture = GRANIT_NULL_HANDLE;
  granit_texture_view output_view = GRANIT_NULL_HANDLE;
  granit_texture_desc output_desc = GRANIT_TEXTURE_DESC_INIT;
  output_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  output_desc.usage =
      GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT;
  output_desc.width = 16;
  output_desc.height = 16;
  REQUIRE(granit_texture_create_with_default_view(renderer.native_handle(), &output_desc,
                                                  &output_texture, &output_view) == GRANIT_SUCCESS);
  granit::buffer readback;
  REQUIRE(readback.initialize(renderer.native_handle(),
                              {.size = 16 * 16 * 4,
                               .usage = granit::buffer_usage::transfer_destination,
                               .location = granit::memory_location::readback}) ==
          granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.bind_graphics_pipeline(resources.pipeline()) == granit::result::success);
  const auto group = resources.group();
  REQUIRE(recorder.bind_graphics_groups(resources.pipeline_layout(), 0, std::span{&group, 1}) ==
          granit::result::success);
  const granit::viewport viewport{0, 0, 16, 16, 0, 1};
  const granit::scissor scissor{0, 0, 16, 16};
  REQUIRE(recorder.set_viewports(0, std::span{&viewport, 1}) == granit::result::success);
  REQUIRE(recorder.set_scissors(0, std::span{&scissor, 1}) == granit::result::success);
  const granit::color_attachment_desc color{.view = output_view};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, 16, 16}};
  REQUIRE(recorder.begin_rendering(rendering) == granit::result::success);
  REQUIRE(recorder.draw(3) == granit::result::success);
  REQUIRE(recorder.end_rendering() == granit::result::success);
  const granit_texture_data_layout copy_layout{};
  const granit_texture_write_region copy_region{.mip_level = 0,
                                                .base_array_layer = 0,
                                                .array_layer_count = 1,
                                                .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                                .x = 0,
                                                .y = 0,
                                                .z = 0,
                                                .width = 16,
                                                .height = 16,
                                                .depth = 1};
  REQUIRE(recorder.copy_texture_to_buffer(output_texture, readback.native_handle(), copy_layout,
                                          copy_region) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  // submit 是异步的；reset 等待该 Recorder 完成后才可安全读取 Readback Buffer。
  REQUIRE(recorder.reset() == granit::result::success);

  granit::math::float3 expected{};
  REQUIRE(granit::lighting::evaluate_tone_mapping(
              {4.0F, 1.0F, 0.25F},
              {.exposure_ev = 1.0F,
               .output_transfer = granit::lighting::tone_mapping_output_transfer::shader_srgb},
              expected) == granit::lighting::tone_mapping_error::none);
  void* mapped = nullptr;
  REQUIRE(readback.map(0, 16 * 16 * 4, &mapped) == granit::result::success);
  const auto* pixel = static_cast<const std::uint8_t*>(mapped) + (8 * 16 + 8) * 4;
  const auto quantize = [](float value) {
    return static_cast<std::uint8_t>(std::lround(value * 255.0F));
  };
  CHECK(pixel[0] == Catch::Approx(quantize(expected.x)).margin(1));
  CHECK(pixel[1] == Catch::Approx(quantize(expected.y)).margin(1));
  CHECK(pixel[2] == Catch::Approx(quantize(expected.z)).margin(1));
  CHECK(pixel[3] == 255);
  REQUIRE(readback.unmap() == granit::result::success);
  REQUIRE(granit_texture_view_destroy(renderer.native_handle(), output_view) == GRANIT_SUCCESS);
  REQUIRE(granit_texture_destroy(renderer.native_handle(), output_texture) == GRANIT_SUCCESS);
}
