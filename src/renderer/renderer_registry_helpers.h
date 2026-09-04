// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_REGISTRY_HELPERS_H_
#define GRANIT_RENDERER_RENDERER_REGISTRY_HELPERS_H_

#include <cstdint>

#include <granit/renderer/texture.h>

#include "backend/contracts/queue.h"

namespace granit::detail {

template <typename Resources, typename Resource, typename Metadata>
void retain_resource(Resources& resources, const Resource& resource, Metadata& metadata) {
  for (const auto& retained : resources) {
    if (retained.resource.get() == resource.get())
      return;
  }
  resources.push_back({.resource = resource, .metadata = &metadata});
}

template <typename Resources>
void mark_resources_used(Resources& resources, submission_serial serial) noexcept {
  for (auto& retained : resources) {
    auto current = retained.metadata->last_use_serial.load();
    while (current < serial &&
           !retained.metadata->last_use_serial.compare_exchange_weak(current, serial)) {
    }
  }
}

[[nodiscard]] inline bool ranges_overlap(std::uint64_t left_offset, std::uint64_t left_size,
                                         std::uint64_t right_offset,
                                         std::uint64_t right_size) noexcept {
  return left_offset < right_offset + right_size && right_offset < left_offset + left_size;
}

[[nodiscard]] inline bool depth_format(granit_texture_format format) noexcept {
  return format >= GRANIT_TEXTURE_FORMAT_D16_UNORM;
}

[[nodiscard]] inline bool stencil_format(granit_texture_format format) noexcept {
  return format == GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT ||
         format == GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT;
}

} // namespace granit::detail

#endif
