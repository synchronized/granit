// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "standard_pbr_assets.h"

#include <cstdint>

namespace granit::example::gltf_rendering {
namespace {

alignas(std::uint32_t) constexpr std::uint8_t archive_bytes[]{
#include "standard_pbr.grmat.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t shader_library_bytes[]{
#include "pbr_standard.grshlib.inc"
};

#if defined(GRANIT_EXAMPLE_HAS_RESOURCE_TABLE_SHADER_LIBRARY)
alignas(std::uint32_t) constexpr std::uint8_t resource_table_shader_library_bytes[]{
#include "pbr_resource_table.grshlib.inc"
};
#endif

} // namespace

std::span<const std::byte> standard_pbr_material_archive() noexcept {
  return {reinterpret_cast<const std::byte*>(archive_bytes), sizeof(archive_bytes)};
}

std::span<const std::byte> standard_pbr_shader_library() noexcept {
  return {reinterpret_cast<const std::byte*>(shader_library_bytes), sizeof(shader_library_bytes)};
}

std::span<const std::byte> standard_pbr_resource_table_shader_library() noexcept {
#if defined(GRANIT_EXAMPLE_HAS_RESOURCE_TABLE_SHADER_LIBRARY)
  return {reinterpret_cast<const std::byte*>(resource_table_shader_library_bytes),
          sizeof(resource_table_shader_library_bytes)};
#else
  return {};
#endif
}

} // namespace granit::example::gltf_rendering
