// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_SHADER_VALIDATION_H_
#define GRANIT_RENDERER_SHADER_VALIDATION_H_

#include <cstdint>
#include <cstring>

#include <granit/renderer/shader.h>

namespace granit::detail {

inline constexpr std::uint32_t spirv_magic = UINT32_C(0x07230203);
inline constexpr std::uint64_t maximum_shader_size = UINT64_C(64) * UINT64_C(1024) * UINT64_C(1024);
inline constexpr std::uint32_t maximum_shader_entry_point_length = UINT32_C(255);

[[nodiscard]] inline granit_result
validate_shader_desc_common(const granit_shader_desc* desc) noexcept {
  if (desc == nullptr || desc->struct_size < GRANIT_SHADER_DESC_VERSION_1_SIZE ||
      desc->entry_point == nullptr || desc->entry_point_length == 0 ||
      desc->entry_point_length > maximum_shader_entry_point_length || desc->reserved != 0 ||
      desc->stage < GRANIT_SHADER_STAGE_VERTEX || desc->stage > GRANIT_SHADER_STAGE_COMPUTE ||
      std::memchr(desc->entry_point, '\0', desc->entry_point_length) != nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_SUCCESS;
}

[[nodiscard]] inline granit_result validate_shader_spirv(const granit_shader_desc* desc) noexcept {
  const auto common = validate_shader_desc_common(desc);
  if (common != GRANIT_SUCCESS) {
    return common;
  }
  if (desc->code == nullptr || desc->code_size < sizeof(std::uint32_t) * 5 ||
      desc->code_size > maximum_shader_size || desc->code_size % sizeof(std::uint32_t) != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  std::uint32_t magic{};
  std::memcpy(&magic, desc->code, sizeof(magic));
  return magic == spirv_magic ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
}

[[nodiscard]] inline granit_result validate_shader_wgsl(const granit_shader_desc* desc) noexcept {
  const auto common = validate_shader_desc_common(desc);
  if (common != GRANIT_SUCCESS) {
    return common;
  }
  if (desc->struct_size < GRANIT_SHADER_DESC_VERSION_2_SIZE || desc->wgsl == nullptr ||
      desc->wgsl_length == 0 || desc->wgsl_length > maximum_shader_size ||
      std::memchr(desc->wgsl, '\0', static_cast<std::size_t>(desc->wgsl_length)) != nullptr ||
      desc->stage == GRANIT_SHADER_STAGE_COMPUTE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_SUCCESS;
}

} // namespace granit::detail

#endif
