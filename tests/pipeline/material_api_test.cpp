// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/texture.hpp>

#include "material/material_package_archive.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <vector>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

std::vector<std::byte> build_archive(bool include_resources = true) {
  using namespace granit::material;
  material_package_desc desc;
  desc.metadata.constant_buffer_size = 16;
  desc.metadata.parameters = {
      {.name = "color", .type = parameter_type::float4, .default_value = {}},
  };
  if (include_resources) {
    desc.metadata.parameters.push_back({.name = "albedo",
                                        .type = parameter_type::texture_view,
                                        .binding = 1,
                                        .default_value = {}});
    desc.metadata.parameters.push_back({.name = "linear_sampler",
                                        .type = parameter_type::sampler,
                                        .binding = 2,
                                        .default_value = {}});
  }
  constexpr std::array spirv{UINT32_C(0x07230203), UINT32_C(0x00010600), 0U, 1U, 0U};
  desc.variants.push_back({.pass = make_feature_id("opaque"),
                           .features = {},
                           .shaders = {{.stage = package_shader_stage::vertex,
                                        .entry_point = "main",
                                        .spirv = {spirv.begin(), spirv.end()},
                                        .wgsl = "@vertex fn main() -> @builtin(position) vec4f { "
                                                "return vec4f(); }"},
                                       {.stage = package_shader_stage::fragment,
                                        .entry_point = "main",
                                        .spirv = {spirv.begin(), spirv.end()},
                                        .wgsl = "@fragment fn main() {}"}},
                           .pipeline = {}});
  material_package package;
  REQUIRE(material_package::build(std::move(desc), package) == package_error::none);
  std::vector<std::byte> archive;
  REQUIRE(encode_material_package_archive(package, archive) == archive_error::none);
  return archive;
}

} // namespace

TEST_CASE("公共Material批量更新保持事务语义") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-public-material"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::texture texture;
  granit::texture_view view;
  const granit::texture_desc texture_desc{.format = granit::texture_format::rgba8_unorm,
                                          .usage = granit::texture_usage::sampled,
                                          .width = 4,
                                          .height = 4};
  REQUIRE(texture.initialize(renderer.native_handle(), texture_desc) == granit::result::success);
  REQUIRE(view.initialize(renderer.native_handle(), texture.native_handle()) ==
          granit::result::success);
  granit::sampler sampler;
  REQUIRE(sampler.initialize(renderer.native_handle(), {}) == granit::result::success);

  const auto archive = build_archive();
  const auto color = std::bit_cast<std::array<std::byte, 16>>(std::array{1.0F, 0.0F, 0.0F, 1.0F});
  std::array updates{
      granit_material_parameter_update{granit::material_parameter_id("color"),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT4, 0, color.data(),
                                       color.size(), GRANIT_NULL_HANDLE},
      granit_material_parameter_update{granit::material_parameter_id("albedo"),
                                       GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                       view.native_handle()},
      granit_material_parameter_update{granit::material_parameter_id("linear_sampler"),
                                       GRANIT_MATERIAL_PARAMETER_SAMPLER, 0, nullptr, 0,
                                       sampler.native_handle()},
  };
  granit_material_desc desc = GRANIT_MATERIAL_DESC_INIT;
  desc.archive_data = archive.data();
  desc.archive_size = archive.size();
  desc.initial_updates = updates.data();
  desc.initial_update_count = static_cast<std::uint32_t>(updates.size());
  granit::material_instance material;
  REQUIRE(material.initialize(renderer.native_handle(), desc) == granit::result::success);

  auto invalid = updates.front();
  invalid.type = GRANIT_MATERIAL_PARAMETER_FLOAT3;
  CHECK(material.update(std::span{&invalid, 1}) == granit::result::invalid_argument);
  const auto blue = std::bit_cast<std::array<std::byte, 16>>(std::array{0.0F, 0.0F, 1.0F, 1.0F});
  auto valid = updates.front();
  valid.data = blue.data();
  CHECK(material.update(std::span{&valid, 1}) == granit::result::success);

  const auto old = material.native_handle();
  REQUIRE(material.reset() == granit::result::success);
  CHECK(granit_material_destroy(renderer.native_handle(), old) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("公共Material拒绝跨Renderer操作与损坏归档") {
  granit::renderer first;
  granit::renderer second;
  const auto first_result = first.initialize({.application_name = "granit-material-first"});
  const auto second_result = second.initialize({.application_name = "granit-material-second"});
  if (environment_unavailable(first_result) || environment_unavailable(second_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(first_result == granit::result::success);
  REQUIRE(second_result == granit::result::success);

  const std::array<std::byte, 4> invalid_archive{};
  granit_material_desc desc = GRANIT_MATERIAL_DESC_INIT;
  desc.archive_data = invalid_archive.data();
  desc.archive_size = invalid_archive.size();
  granit_material material = UINT64_C(123);
  CHECK(granit_material_create(first.native_handle(), &desc, &material) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(material == GRANIT_NULL_HANDLE);

  const auto archive = build_archive(false);
  const auto color = std::bit_cast<std::array<std::byte, 16>>(std::array{1.0F, 1.0F, 1.0F, 1.0F});
  const granit_material_parameter_update update{granit::material_parameter_id("color"),
                                                GRANIT_MATERIAL_PARAMETER_FLOAT4,
                                                0,
                                                color.data(),
                                                color.size(),
                                                GRANIT_NULL_HANDLE};
  desc.archive_data = archive.data();
  desc.archive_size = archive.size();
  desc.initial_updates = &update;
  desc.initial_update_count = 1;
  REQUIRE(granit_material_create(first.native_handle(), &desc, &material) == GRANIT_SUCCESS);
  CHECK(granit_material_destroy(second.native_handle(), material) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(granit_material_destroy(first.native_handle(), material) == GRANIT_SUCCESS);
  granit_material replacement = GRANIT_NULL_HANDLE;
  REQUIRE(granit_material_create(first.native_handle(), &desc, &replacement) == GRANIT_SUCCESS);
  CHECK(replacement != material);
  CHECK(granit_material_update(first.native_handle(), material, &update, 1) ==
        GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(granit_material_destroy(first.native_handle(), replacement) == GRANIT_SUCCESS);
}
