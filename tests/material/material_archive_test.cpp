// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_archive.h"

#include <catch2/catch_all.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace granit::material;

void write_u32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) {
  for (std::uint32_t index = 0; index < 4; ++index) {
    bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
  }
}

void write_u64(std::span<std::byte> bytes, std::size_t offset, std::uint64_t value) {
  write_u32(bytes, offset, static_cast<std::uint32_t>(value));
  write_u32(bytes, offset + 4, static_cast<std::uint32_t>(value >> 32U));
}

std::vector<std::byte> make_archive() {
  constexpr std::uint32_t section_count = 8;
  constexpr std::uint64_t directory_offset = material_archive_header_size;
  constexpr std::uint64_t data_offset =
      directory_offset + section_count * material_archive_section_record_size;
  std::vector<std::byte> bytes(data_offset + section_count);
  bytes[0] = std::byte{'G'};
  bytes[1] = std::byte{'R'};
  bytes[2] = std::byte{'M'};
  bytes[3] = std::byte{'A'};
  bytes[4] = std::byte{'T'};
  write_u32(bytes, 8, material_archive_version_major);
  write_u32(bytes, 12, material_archive_version_minor);
  write_u32(bytes, 16, material_archive_endian_tag);
  write_u32(bytes, 20, material_archive_header_size);
  write_u32(bytes, 24, material_archive_section_record_size);
  write_u32(bytes, 28, section_count);
  write_u64(bytes, 32, directory_offset);
  write_u64(bytes, 40, bytes.size());
  write_u32(bytes, 48, material_archive_target_vulkan_1_3);
  write_u32(bytes, 52, material_archive_binding_model_bind_group);
  for (std::uint32_t index = 0; index < section_count; ++index) {
    const auto record = directory_offset + index * material_archive_section_record_size;
    write_u32(bytes, record, index + 1);
    write_u32(bytes, record + 4, index < 7 ? archive_section_required : 0);
    write_u64(bytes, record + 8, data_offset + index);
    write_u64(bytes, record + 16, 1);
    write_u64(bytes, record + 24, 1);
    write_u32(bytes, record + 32, 1);
  }
  return bytes;
}

} // namespace

TEST_CASE("材质归档解析定宽文件头和区段目录") {
  const auto bytes = make_archive();
  material_archive_layout layout;
  REQUIRE(parse_material_archive_layout(bytes, layout) == archive_error::none);
  CHECK(layout.header.version_major == material_archive_version_major);
  CHECK(layout.header.file_size == bytes.size());
  REQUIRE(layout.sections.size() == 8);
  CHECK(layout.sections.front().type ==
        static_cast<std::uint32_t>(archive_section_type::string_table));
}

TEST_CASE("材质归档拒绝截断、错误字节序和区段重叠") {
  material_archive_layout layout;
  auto bytes = make_archive();
  CHECK(parse_material_archive_layout(std::span{bytes}.first(32), layout) ==
        archive_error::truncated);

  bytes = make_archive();
  write_u32(bytes, 16, UINT32_C(0x04030201));
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::unsupported_endianness);

  bytes = make_archive();
  const auto first_offset =
      material_archive_header_size + UINT64_C(8) * material_archive_section_record_size;
  write_u64(bytes, material_archive_header_size + material_archive_section_record_size + 8,
            first_offset);
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::overlapping_sections);
}

TEST_CASE("材质归档跳过未知可选区段并拒绝未知必需区段") {
  material_archive_layout layout;
  auto bytes = make_archive();
  const auto optional_record =
      material_archive_header_size + UINT64_C(7) * material_archive_section_record_size;
  write_u32(bytes, optional_record, 100);
  REQUIRE(parse_material_archive_layout(bytes, layout) == archive_error::none);

  write_u32(bytes, optional_record + 4, archive_section_required);
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::unknown_required_section);
}
