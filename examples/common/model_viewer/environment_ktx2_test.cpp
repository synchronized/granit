// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/environment_ktx2.h"

#include <catch2/catch_all.hpp>

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

std::vector<std::byte> valid_ktx2() {
  constexpr std::array<std::uint8_t, 12> identifier{0xab, 0x4b, 0x54, 0x58, 0x20, 0x32,
                                                    0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
  constexpr std::uint64_t level_zero_size = 4 * 4 * 6 * 8;
  constexpr std::uint64_t level_one_size = 2 * 2 * 6 * 8;
  constexpr std::uint64_t data_offset = 80 + 2 * 24;
  std::vector<std::byte> bytes(data_offset + level_zero_size + level_one_size);
  for (std::size_t index = 0; index < identifier.size(); ++index)
    bytes[index] = static_cast<std::byte>(identifier[index]);
  write_u32(bytes, 12, 97);
  write_u32(bytes, 16, 2);
  write_u32(bytes, 20, 4);
  write_u32(bytes, 24, 4);
  write_u32(bytes, 36, 6);
  write_u32(bytes, 40, 2);
  write_u64(bytes, 80, data_offset);
  write_u64(bytes, 88, level_zero_size);
  write_u64(bytes, 96, level_zero_size);
  write_u64(bytes, 104, data_offset + level_zero_size);
  write_u64(bytes, 112, level_one_size);
  write_u64(bytes, 120, level_one_size);
  return bytes;
}

} // namespace

TEST_CASE("环境工具只接受未压缩RGBA16F KTX2 Cube", "[example][model-viewer][environment]") {
  using namespace granit::example::model_viewer;
  auto bytes = valid_ktx2();
  environment_ktx2_cube cube;
  REQUIRE(parse_environment_ktx2_cube(bytes, cube) == environment_ktx2_error::none);
  REQUIRE(cube.levels.size() == 2);
  CHECK(cube.levels[0].resolution == 4);
  CHECK(cube.levels[1].resolution == 2);

  bytes[0] = std::byte{0};
  CHECK(parse_environment_ktx2_cube(bytes, cube) == environment_ktx2_error::invalid_identifier);
  bytes = valid_ktx2();
  write_u32(bytes, 44, 1);
  CHECK(parse_environment_ktx2_cube(bytes, cube) == environment_ktx2_error::unsupported_layout);
  bytes = valid_ktx2();
  write_u32(bytes, 36, 1);
  CHECK(parse_environment_ktx2_cube(bytes, cube) == environment_ktx2_error::unsupported_layout);
  bytes = valid_ktx2();
  write_u64(bytes, 88, 8);
  CHECK(parse_environment_ktx2_cube(bytes, cube) == environment_ktx2_error::invalid_level);
}
