// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_archive.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
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

void refresh_hash(std::span<std::byte> bytes) {
  const auto hash = calculate_material_archive_content_hash(bytes);
  std::ranges::copy(hash, bytes.begin() + 64);
}

std::vector<std::byte> make_archive() {
  constexpr std::uint32_t section_count = 10;
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
  write_u32(bytes, 48, material_archive_target_cross_backend);
  write_u32(bytes, 52, material_archive_binding_model_bind_group);
  for (std::uint32_t index = 0; index < section_count; ++index) {
    const auto record = directory_offset + index * material_archive_section_record_size;
    write_u32(bytes, record,
              index < 7 ? index + 1 : (index == 7 ? 10 : (index == 8 ? 11 : 8)));
    write_u32(bytes, record + 4, index < 9 ? archive_section_required : 0);
    write_u64(bytes, record + 8, data_offset + index);
    write_u64(bytes, record + 16, 1);
    write_u64(bytes, record + 24, 1);
    write_u32(bytes, record + 32, 1);
  }
  refresh_hash(bytes);
  return bytes;
}

} // namespace

TEST_CASE("材质归档解析定宽文件头和区段目录") {
  const auto bytes = make_archive();
  material_archive_layout layout;
  REQUIRE(parse_material_archive_layout(bytes, layout) == archive_error::none);
  CHECK(layout.header.version_major == material_archive_version_major);
  CHECK(layout.header.file_size == bytes.size());
  REQUIRE(layout.sections.size() == 10);
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
      material_archive_header_size + UINT64_C(10) * material_archive_section_record_size;
  write_u64(bytes, material_archive_header_size + material_archive_section_record_size + 8,
            first_offset);
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::overlapping_sections);
}

TEST_CASE("材质归档跳过未知可选区段并拒绝未知必需区段") {
  material_archive_layout layout;
  auto bytes = make_archive();
  const auto optional_record =
      material_archive_header_size + UINT64_C(9) * material_archive_section_record_size;
  write_u32(bytes, optional_record, 100);
  refresh_hash(bytes);
  REQUIRE(parse_material_archive_layout(bytes, layout) == archive_error::none);

  write_u32(bytes, optional_record + 4, archive_section_required);
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::unknown_required_section);
}

TEST_CASE("材质归档编码结果确定且能够重新解析") {
  const std::array<std::byte, 1> value{std::byte{0x2a}};
  std::array<material_archive_section_source, 9> sections{};
  for (std::size_t index = 0; index < sections.size(); ++index) {
    const auto type = index == 0   ? archive_section_type::wgsl_data
                      : index == 1 ? archive_section_type::pipeline_states
                                   : static_cast<archive_section_type>(9 - index);
    sections[index] = {.type = type,
                       .flags = archive_section_required,
                       .alignment = index == 1 ? 16U : 1U,
                       .bytes = value};
  }
  std::vector<std::byte> first;
  std::vector<std::byte> second;
  REQUIRE(encode_material_archive({.sections = sections}, first) == archive_error::none);
  auto reordered = sections;
  std::ranges::reverse(reordered);
  REQUIRE(encode_material_archive({.sections = reordered}, second) == archive_error::none);
  CHECK(first == second);

  material_archive_layout layout;
  REQUIRE(parse_material_archive_layout(first, layout) == archive_error::none);
  REQUIRE(layout.sections.size() == sections.size());
  CHECK(layout.sections.front().type == 1);
  CHECK(layout.sections.back().type == 11);
}

TEST_CASE("材质归档检测内容篡改") {
  auto bytes = make_archive();
  bytes.back() ^= std::byte{1};
  material_archive_layout layout;
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::content_hash_mismatch);
}

TEST_CASE("材质归档拒绝非法头部、目录与区段范围") {
  material_archive_layout layout;
  auto bytes = make_archive();
  bytes[0] = std::byte{'X'};
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::invalid_magic);

  bytes = make_archive();
  write_u32(bytes, 28, material_archive_max_sections + 1);
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::too_many_sections);

  bytes = make_archive();
  write_u64(bytes, 32, material_archive_header_size + 8);
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::invalid_directory);

  bytes = make_archive();
  write_u32(bytes, material_archive_header_size + 32, 3);
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::invalid_section);

  bytes = make_archive();
  write_u64(bytes, material_archive_header_size + 16, UINT64_MAX);
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::invalid_section);
}

TEST_CASE("材质归档拒绝未声明的尾随数据") {
  auto bytes = make_archive();
  bytes.push_back(std::byte{0});
  write_u64(bytes, 40, bytes.size());
  refresh_hash(bytes);
  material_archive_layout layout;
  CHECK(parse_material_archive_layout(bytes, layout) == archive_error::invalid_section);
}

TEST_CASE("材质归档任意单字节损坏均不会被接受") {
  const auto source = make_archive();
  for (std::size_t index = 0; index < source.size(); ++index) {
    auto bytes = source;
    bytes[index] ^= std::byte{0x5a};
    material_archive_layout layout;
    CAPTURE(index);
    CHECK(parse_material_archive_layout(bytes, layout) != archive_error::none);
  }
}

TEST_CASE("材质归档 SHA-256 符合标准测试向量") {
  constexpr std::string_view input = "abc";
  constexpr std::array<std::uint8_t, 32> expected{0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
                                                  0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                                                  0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
                                                  0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  const auto bytes = std::as_bytes(std::span{input.data(), input.size()});
  const auto hash = calculate_material_archive_content_hash(bytes);
  for (std::size_t index = 0; index < hash.size(); ++index) {
    CHECK(std::to_integer<std::uint8_t>(hash[index]) == expected[index]);
  }
}
