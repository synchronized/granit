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

} // namespace

std::span<const std::byte> standard_pbr_material_archive() noexcept {
  return {reinterpret_cast<const std::byte*>(archive_bytes), sizeof(archive_bytes)};
}

std::span<const std::byte> standard_pbr_shader_library() noexcept {
  return {reinterpret_cast<const std::byte*>(shader_library_bytes), sizeof(shader_library_bytes)};
}

} // namespace granit::example::gltf_rendering
