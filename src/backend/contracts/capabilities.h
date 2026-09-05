// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_CAPABILITIES_H_
#define GRANIT_BACKEND_CAPABILITIES_H_

#include <cstdint>

namespace granit::detail {

enum class backend_buffer_binding_type { uniform, storage };

/** Renderer 创建后固定的后端无关能力快照。 */
struct backend_capabilities {
  std::uint64_t uniform_buffer_offset_alignment{1};
  std::uint64_t storage_buffer_offset_alignment{1};
  std::uint64_t max_uniform_buffer_binding_size{};
  std::uint64_t max_storage_buffer_binding_size{};
  std::uint32_t framebuffer_sample_counts{1};
  float max_sampler_anisotropy{1.0F};
  std::uint64_t shader_features{};
  std::uint32_t shader_profile{1};

  [[nodiscard]] bool supports_buffer_binding(backend_buffer_binding_type type, std::uint64_t offset,
                                             std::uint64_t size) const noexcept {
    const auto alignment = type == backend_buffer_binding_type::uniform
                               ? uniform_buffer_offset_alignment
                               : storage_buffer_offset_alignment;
    const auto maximum_size = type == backend_buffer_binding_type::uniform
                                  ? max_uniform_buffer_binding_size
                                  : max_storage_buffer_binding_size;
    return size != 0 && (alignment == 0 || offset % alignment == 0) && size <= maximum_size;
  }
};

} // namespace granit::detail

#endif
