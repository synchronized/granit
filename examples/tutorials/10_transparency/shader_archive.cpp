// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
#include "shader_archive.h"
#include <cstdint>
namespace tutorial_transparency {
namespace { alignas(std::uint32_t) constexpr std::uint8_t bytes[]{
#include "transparency.grshlib.inc"
}; }
std::span<const std::byte> shader_archive() noexcept {
  return {reinterpret_cast<const std::byte*>(bytes), sizeof(bytes)};
}
} // namespace tutorial_transparency
