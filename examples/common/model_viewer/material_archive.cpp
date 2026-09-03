// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/material_archive.h"

#include <cstdint>

namespace granit::example::model_viewer {
namespace {

alignas(std::uint32_t) constexpr std::uint8_t archive_bytes[]{
#include "model_viewer_pbr.grmat.inc"
};

} // namespace

std::span<const std::byte> model_viewer_material_archive() noexcept {
  return {reinterpret_cast<const std::byte*>(archive_bytes), sizeof(archive_bytes)};
}

} // namespace granit::example::model_viewer
