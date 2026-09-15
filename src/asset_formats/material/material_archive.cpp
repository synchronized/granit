// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_formats/material/material_archive.h"

#include "core/sha256.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <new>

namespace granit::material {
namespace {

constexpr std::array<std::byte, 8> archive_magic{std::byte{'G'}, std::byte{'R'}, std::byte{'M'},
                                                 std::byte{'A'}, std::byte{'T'}, std::byte{0},
                                                 std::byte{0},   std::byte{0}};
constexpr std::uint32_t first_required_section = 1;
constexpr std::uint32_t last_known_section = 11;

bool required_section(std::uint32_t type) noexcept {
  return (type >= first_required_section && type <= 6) || type == 10;
}

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  return std::to_integer<std::uint32_t>(bytes[offset]) |
         (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 8U) |
         (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 16U) |
         (std::to_integer<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::uint64_t read_u64(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  return read_u32(bytes, offset) | (static_cast<std::uint64_t>(read_u32(bytes, offset + 4)) << 32U);
}

bool range_valid(std::uint64_t offset, std::uint64_t size, std::uint64_t limit) noexcept {
  return offset <= limit && size <= limit - offset;
}

bool power_of_two(std::uint32_t value) noexcept {
  return value != 0 && (value & (value - 1U)) == 0;
}

void write_u32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) noexcept {
  for (std::uint32_t index = 0; index < 4; ++index) {
    bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
  }
}

void write_u64(std::span<std::byte> bytes, std::size_t offset, std::uint64_t value) noexcept {
  write_u32(bytes, offset, static_cast<std::uint32_t>(value));
  write_u32(bytes, offset + 4, static_cast<std::uint32_t>(value >> 32U));
}

std::uint64_t align_up(std::uint64_t value, std::uint32_t alignment) noexcept {
  const auto mask = static_cast<std::uint64_t>(alignment - 1U);
  return (value + mask) & ~mask;
}

} // namespace

material_archive_hash
calculate_material_archive_content_hash(std::span<const std::byte> bytes) noexcept {
  if (bytes.size() < material_archive_header_size)
    return granit::detail::sha256_bytes(bytes);
  return granit::detail::sha256_bytes_with_zeroed_range(bytes, 64, 32);
}

archive_error encode_material_archive(const material_archive_encode_desc& desc,
                                      std::vector<std::byte>& bytes) noexcept {
  if (desc.target_environment != material_archive_target_cross_backend) {
    return archive_error::unsupported_target;
  }
  if (desc.binding_model != material_archive_binding_model_bind_group) {
    return archive_error::unsupported_binding_model;
  }
  if (desc.required_renderer_features != 0) {
    return archive_error::unsupported_renderer_features;
  }
  if (desc.sections.size() > material_archive_max_sections) {
    return archive_error::too_many_sections;
  }
  try {
    std::vector<material_archive_section_source> sources(desc.sections.begin(),
                                                         desc.sections.end());
    std::ranges::sort(sources, {},
                      [](const auto& source) { return static_cast<std::uint32_t>(source.type); });
    std::array<bool, last_known_section + 1> seen{};
    for (const auto& source : sources) {
      const auto type = static_cast<std::uint32_t>(source.type);
      if (type == 0 || type > last_known_section || seen[type]) {
        return type > last_known_section ? archive_error::invalid_section
                                         : archive_error::duplicate_section;
      }
      seen[type] = true;
      if (required_section(type) && (source.flags & archive_section_required) == 0) {
        return archive_error::invalid_section;
      }
      if ((source.flags & ~archive_section_required) != 0 || !power_of_two(source.alignment) ||
          source.alignment > material_archive_max_alignment || source.bytes.empty()) {
        return archive_error::invalid_section;
      }
    }
    for (std::uint32_t type = first_required_section; type <= 6; ++type) {
      if (!seen[type]) {
        return archive_error::missing_required_section;
      }
    }
    if (!seen[static_cast<std::uint32_t>(archive_section_type::pipeline_states)]) {
      return archive_error::missing_required_section;
    }

    const auto directory_size = sources.size() * material_archive_section_record_size;
    auto cursor = static_cast<std::uint64_t>(material_archive_header_size + directory_size);
    std::vector<material_archive_section> sections;
    sections.reserve(sources.size());
    for (const auto& source : sources) {
      cursor = align_up(cursor, source.alignment);
      if (cursor > material_archive_max_file_size ||
          source.bytes.size() > material_archive_max_file_size - cursor) {
        return archive_error::invalid_file_size;
      }
      sections.push_back({.type = static_cast<std::uint32_t>(source.type),
                          .flags = source.flags,
                          .offset = cursor,
                          .stored_size = source.bytes.size(),
                          .unpacked_size = source.bytes.size(),
                          .alignment = source.alignment,
                          .checksum = 0});
      cursor += source.bytes.size();
    }
    std::vector<std::byte> encoded(static_cast<std::size_t>(cursor));
    std::ranges::copy(archive_magic, encoded.begin());
    write_u32(encoded, 8, material_archive_version_major);
    write_u32(encoded, 12, material_archive_version_minor);
    write_u32(encoded, 16, material_archive_endian_tag);
    write_u32(encoded, 20, material_archive_header_size);
    write_u32(encoded, 24, material_archive_section_record_size);
    write_u32(encoded, 28, static_cast<std::uint32_t>(sections.size()));
    write_u64(encoded, 32, material_archive_header_size);
    write_u64(encoded, 40, encoded.size());
    write_u32(encoded, 48, desc.target_environment);
    write_u32(encoded, 52, desc.binding_model);
    write_u64(encoded, 56, desc.required_renderer_features);
    for (std::size_t index = 0; index < sections.size(); ++index) {
      const auto record =
          material_archive_header_size + index * material_archive_section_record_size;
      const auto& section = sections[index];
      write_u32(encoded, record, section.type);
      write_u32(encoded, record + 4, section.flags);
      write_u64(encoded, record + 8, section.offset);
      write_u64(encoded, record + 16, section.stored_size);
      write_u64(encoded, record + 24, section.unpacked_size);
      write_u32(encoded, record + 32, section.alignment);
      write_u32(encoded, record + 36, section.checksum);
      std::ranges::copy(sources[index].bytes,
                        encoded.begin() + static_cast<std::ptrdiff_t>(section.offset));
    }
    const auto hash = calculate_material_archive_content_hash(encoded);
    std::ranges::copy(hash, encoded.begin() + 64);
    bytes = std::move(encoded);
    return archive_error::none;
  } catch (const std::bad_alloc&) {
    return archive_error::out_of_memory;
  } catch (...) {
    return archive_error::out_of_memory;
  }
}

archive_error parse_material_archive_layout(std::span<const std::byte> bytes,
                                            material_archive_layout& layout) noexcept {
  if (bytes.size() < material_archive_header_size) {
    return archive_error::truncated;
  }
  if (!std::ranges::equal(bytes.first(archive_magic.size()), archive_magic)) {
    return archive_error::invalid_magic;
  }

  material_archive_layout parsed;
  parsed.header.version_major = read_u32(bytes, 8);
  parsed.header.version_minor = read_u32(bytes, 12);
  if (parsed.header.version_major != material_archive_version_major ||
      parsed.header.version_minor != material_archive_version_minor) {
    return archive_error::unsupported_version;
  }
  if (read_u32(bytes, 16) != material_archive_endian_tag) {
    return archive_error::unsupported_endianness;
  }
  if (read_u32(bytes, 20) != material_archive_header_size ||
      read_u32(bytes, 24) != material_archive_section_record_size) {
    return archive_error::invalid_header;
  }

  const auto section_count = read_u32(bytes, 28);
  if (section_count > material_archive_max_sections) {
    return archive_error::too_many_sections;
  }
  const auto directory_offset = read_u64(bytes, 32);
  parsed.header.file_size = read_u64(bytes, 40);
  if (parsed.header.file_size != bytes.size()) {
    return archive_error::invalid_file_size;
  }
  if (parsed.header.file_size > material_archive_max_file_size) {
    return archive_error::invalid_file_size;
  }
  const auto directory_size =
      static_cast<std::uint64_t>(section_count) * material_archive_section_record_size;
  if (directory_offset != material_archive_header_size ||
      !range_valid(directory_offset, directory_size, parsed.header.file_size)) {
    return archive_error::invalid_directory;
  }

  parsed.header.target_environment = read_u32(bytes, 48);
  parsed.header.binding_model = read_u32(bytes, 52);
  parsed.header.required_renderer_features = read_u64(bytes, 56);
  if (parsed.header.target_environment != material_archive_target_cross_backend) {
    return archive_error::unsupported_target;
  }
  if (parsed.header.binding_model != material_archive_binding_model_bind_group) {
    return archive_error::unsupported_binding_model;
  }
  if (parsed.header.required_renderer_features != 0) {
    return archive_error::unsupported_renderer_features;
  }
  std::memcpy(parsed.header.content_hash.data(), bytes.data() + 64,
              parsed.header.content_hash.size());

  try {
    parsed.sections.reserve(section_count);
    std::array<bool, last_known_section + 1> seen{};
    for (std::uint32_t index = 0; index < section_count; ++index) {
      const auto record_offset =
          static_cast<std::size_t>(directory_offset + static_cast<std::uint64_t>(index) *
                                                          material_archive_section_record_size);
      material_archive_section section{.type = read_u32(bytes, record_offset),
                                       .flags = read_u32(bytes, record_offset + 4),
                                       .offset = read_u64(bytes, record_offset + 8),
                                       .stored_size = read_u64(bytes, record_offset + 16),
                                       .unpacked_size = read_u64(bytes, record_offset + 24),
                                       .alignment = read_u32(bytes, record_offset + 32),
                                       .checksum = read_u32(bytes, record_offset + 36)};
      if (section.type == 0 || section.type > last_known_section) {
        if ((section.flags & archive_section_required) != 0) {
          return archive_error::unknown_required_section;
        }
      } else if (seen[section.type]) {
        return archive_error::duplicate_section;
      } else {
        seen[section.type] = true;
      }
      if (required_section(section.type) && (section.flags & archive_section_required) == 0) {
        return archive_error::invalid_section;
      }
      if ((section.flags & archive_section_compressed) != 0) {
        return archive_error::unsupported_compression;
      }
      if ((section.flags & ~(archive_section_required | archive_section_compressed)) != 0 ||
          !power_of_two(section.alignment) || section.alignment > material_archive_max_alignment ||
          section.offset % section.alignment != 0 || section.stored_size == 0 ||
          section.unpacked_size != section.stored_size ||
          !range_valid(section.offset, section.stored_size, parsed.header.file_size)) {
        return archive_error::invalid_section;
      }
      const auto directory_end = directory_offset + directory_size;
      if (section.offset < directory_end) {
        return archive_error::overlapping_sections;
      }
      parsed.sections.push_back(section);
    }
    for (std::uint32_t type = first_required_section; type <= 6; ++type) {
      if (!seen[type]) {
        return archive_error::missing_required_section;
      }
    }
    if (!seen[static_cast<std::uint32_t>(archive_section_type::pipeline_states)]) {
      return archive_error::missing_required_section;
    }
    std::ranges::sort(parsed.sections, {}, &material_archive_section::offset);
    for (std::size_t index = 1; index < parsed.sections.size(); ++index) {
      const auto& previous = parsed.sections[index - 1];
      if (parsed.sections[index].offset < previous.offset + previous.stored_size) {
        return archive_error::overlapping_sections;
      }
    }
    if (parsed.sections.empty() ||
        parsed.sections.back().offset + parsed.sections.back().stored_size !=
            parsed.header.file_size) {
      return archive_error::invalid_section;
    }
  } catch (const std::bad_alloc&) {
    return archive_error::out_of_memory;
  } catch (...) {
    return archive_error::out_of_memory;
  }

  if (calculate_material_archive_content_hash(bytes) != parsed.header.content_hash) {
    return archive_error::content_hash_mismatch;
  }

  layout = std::move(parsed);
  return archive_error::none;
}

} // namespace granit::material
