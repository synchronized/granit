// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_archive.h"

#include <cstdint>

namespace tutorial_metaballs {
namespace {

alignas(std::uint32_t) constexpr std::uint8_t archive_bytes[]{
#include "metaballs.grshlib.inc"
};

} // namespace

std::span<const std::byte> shader_archive() noexcept {
  return {reinterpret_cast<const std::byte*>(archive_bytes), sizeof(archive_bytes)};
}

} // namespace tutorial_metaballs
