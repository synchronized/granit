// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_MATERIAL_ARCHIVE_H
#define GRANIT_MATERIAL_MATERIAL_ARCHIVE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace granit::material {

inline constexpr std::uint32_t material_archive_version_major = 2;
inline constexpr std::uint32_t material_archive_version_minor = 0;
inline constexpr std::uint32_t material_archive_endian_tag = UINT32_C(0x01020304);
inline constexpr std::uint32_t material_archive_header_size = 96;
inline constexpr std::uint32_t material_archive_section_record_size = 40;
inline constexpr std::uint32_t material_archive_max_sections = 64;
inline constexpr std::uint32_t material_archive_max_alignment = 4096;
inline constexpr std::uint64_t material_archive_max_file_size = UINT64_C(1024) * 1024 * 1024;
inline constexpr std::uint32_t material_archive_target_vulkan_1_3 = 1;
inline constexpr std::uint32_t material_archive_binding_model_bind_group = 1;

enum class archive_section_type : std::uint32_t {
  string_table = 1,
  material_metadata = 2,
  feature_definitions = 3,
  pass_definitions = 4,
  variant_records = 5,
  shader_records = 6,
  spirv_data = 7,
  build_metadata = 8,
  dependency_metadata = 9,
  pipeline_states = 10,
};

using archive_section_flags = std::uint32_t;
inline constexpr archive_section_flags archive_section_required = UINT32_C(1) << 0;
inline constexpr archive_section_flags archive_section_compressed = UINT32_C(1) << 1;

enum class archive_error : std::uint8_t {
  none,
  truncated,
  invalid_magic,
  unsupported_version,
  unsupported_endianness,
  invalid_header,
  unsupported_target,
  unsupported_binding_model,
  unsupported_renderer_features,
  invalid_file_size,
  too_many_sections,
  invalid_directory,
  unknown_required_section,
  unsupported_compression,
  invalid_section,
  duplicate_section,
  missing_required_section,
  overlapping_sections,
  content_hash_mismatch,
  invalid_semantic_data,
  out_of_memory,
};

using material_archive_hash = std::array<std::byte, 32>;

struct material_archive_header {
  std::uint32_t version_major = 0;
  std::uint32_t version_minor = 0;
  std::uint64_t file_size = 0;
  std::uint32_t target_environment = 0;
  std::uint32_t binding_model = 0;
  std::uint64_t required_renderer_features = 0;
  material_archive_hash content_hash{};
};

struct material_archive_section {
  std::uint32_t type = 0;
  archive_section_flags flags = 0;
  std::uint64_t offset = 0;
  std::uint64_t stored_size = 0;
  std::uint64_t unpacked_size = 0;
  std::uint32_t alignment = 0;
  std::uint32_t checksum = 0;
};

struct material_archive_layout {
  material_archive_header header;
  std::vector<material_archive_section> sections;
};

struct material_archive_section_source {
  archive_section_type type = archive_section_type::string_table;
  archive_section_flags flags = 0;
  std::uint32_t alignment = 1;
  std::span<const std::byte> bytes;
};

struct material_archive_encode_desc {
  std::uint32_t target_environment = material_archive_target_vulkan_1_3;
  std::uint32_t binding_model = material_archive_binding_model_bind_group;
  std::uint64_t required_renderer_features = 0;
  std::span<const material_archive_section_source> sections;
};

[[nodiscard]] material_archive_hash
calculate_material_archive_content_hash(std::span<const std::byte> bytes) noexcept;
[[nodiscard]] archive_error encode_material_archive(const material_archive_encode_desc& desc,
                                                    std::vector<std::byte>& bytes) noexcept;

[[nodiscard]] archive_error parse_material_archive_layout(std::span<const std::byte> bytes,
                                                          material_archive_layout& layout) noexcept;

} // namespace granit::material

#endif
