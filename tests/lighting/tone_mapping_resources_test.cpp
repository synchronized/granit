// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/tone_mapping_resources.h"

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

#include <catch2/catch_all.hpp>

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
                               {.exposure_scale = 2.0F, .encode_srgb = 1}, vertex, fragment) ==
          GRANIT_SUCCESS);
  CHECK(resources.pipeline() != GRANIT_NULL_HANDLE);
  CHECK(resources.pipeline_layout() != GRANIT_NULL_HANDLE);
  CHECK(resources.group() != GRANIT_NULL_HANDLE);
  CHECK(resources.update({.exposure_scale = 0.5F, .encode_srgb = 0}) == GRANIT_SUCCESS);
  CHECK(resources.update({.exposure_scale = 0.0F}) == GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Tone Mapping GPU资源拒绝不完整输入") {
  granit::lighting::tone_mapping_resources resources;
  CHECK(resources.initialize(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE,
                             granit::texture_format::undefined, {}, {}, {}) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(resources.update({}) == GRANIT_ERROR_INVALID_ARGUMENT);
}
