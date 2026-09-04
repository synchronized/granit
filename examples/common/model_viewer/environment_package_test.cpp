// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/environment_package.h"
#include "model_viewer/environment_resources.h"

#include <catch2/catch_all.hpp>
#include <granit/renderer/renderer.hpp>

#include <array>
#include <vector>

namespace {

void write_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index)
    bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
}

void write_u64(std::vector<std::byte>& bytes, std::size_t offset, std::uint64_t value) {
  for (std::size_t index = 0; index < 8; ++index)
    bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
}

std::vector<std::byte> valid_package() {
  constexpr std::array magic{'G', 'R', 'E', 'N', 'V', '0', '2', '\0'};
  constexpr std::uint64_t payload_size =
      2 * 2 * 6 * 8 + 4 * 4 * 6 * 8 + 2 * 2 * 6 * 8 + 1 * 1 * 6 * 8 + 4 * 4 * 8;
  std::vector<std::byte> bytes(64 + payload_size);
  for (std::size_t index = 0; index < magic.size(); ++index)
    bytes[index] = static_cast<std::byte>(magic[index]);
  write_u32(bytes, 8, 2);
  write_u32(bytes, 12, 64);
  write_u32(bytes, 16, 1);
  write_u32(bytes, 20, 2);
  write_u32(bytes, 24, 4);
  write_u32(bytes, 28, 3);
  write_u32(bytes, 32, 4);
  write_u32(bytes, 36, 4);
  write_u64(bytes, 40, payload_size);
  write_u32(bytes, 48, 0x3df5c28fU);
  write_u32(bytes, 52, 0xbf000000U);
  return bytes;
}

} // namespace

TEST_CASE("GRENV v2严格解析预处理环境布局和推荐光照", "[example][model-viewer][environment]") {
  using namespace granit::example::model_viewer;
  auto bytes = valid_package();
  environment_package package;
  REQUIRE(parse_environment_package(bytes, package) == environment_package_error::none);
  CHECK(package.irradiance_resolution == 2);
  CHECK(package.recommended_environment_intensity == Catch::Approx(0.12F));
  CHECK(package.recommended_exposure_ev == Catch::Approx(-0.5F));
  REQUIRE(package.prefiltered_mips.size() == 3);
  CHECK(package.prefiltered_mips[0].resolution == 4);
  CHECK(package.prefiltered_mips[2].resolution == 1);
  CHECK(package.brdf_width == 4);
  CHECK(package.brdf_pixels.size() == 4 * 4 * 8);

  std::vector<std::byte> encoded;
  REQUIRE(encode_environment_package(package, encoded) == environment_package_error::none);
  CHECK(encoded == valid_package());

  bytes[0] = std::byte{0};
  CHECK(parse_environment_package(bytes, package) == environment_package_error::invalid_magic);
  bytes = valid_package();
  write_u32(bytes, 8, 1);
  CHECK(parse_environment_package(bytes, package) ==
        environment_package_error::unsupported_version);
  bytes = valid_package();
  write_u32(bytes, 48, 0x7f800000U);
  CHECK(parse_environment_package(bytes, package) == environment_package_error::invalid_layout);
  bytes = valid_package();
  write_u32(bytes, 20, 3);
  CHECK(parse_environment_package(bytes, package) == environment_package_error::invalid_layout);
  bytes = valid_package();
  write_u32(bytes, 28, 4);
  CHECK(parse_environment_package(bytes, package) == environment_package_error::invalid_layout);
  bytes = valid_package();
  bytes.pop_back();
  CHECK(parse_environment_package(bytes, package) == environment_package_error::invalid_layout);
}

TEST_CASE("GRENV环境资源上传为Render Pipeline输入", "[example][model-viewer][environment][gpu]") {
  using namespace granit::example::model_viewer;
  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "GRENV Test"});
  if (renderer_result.failed())
    SKIP("当前环境没有可用 Renderer");

  const auto bytes = valid_package();
  environment_package package;
  REQUIRE(parse_environment_package(bytes, package) == environment_package_error::none);
  environment_resources resources;
  REQUIRE(resources.initialize(renderer.native_handle(), package) == granit::result::success);
  CHECK(resources.valid());
  CHECK(resources.environment().irradiance != GRANIT_NULL_HANDLE);
  CHECK(resources.environment().prefiltered_environment != GRANIT_NULL_HANDLE);
  CHECK(resources.environment().brdf_lut != GRANIT_NULL_HANDLE);
  CHECK(resources.environment().prefiltered_max_mip == 2.0F);
  resources.reset();
  CHECK_FALSE(resources.valid());
}
