// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/shader.h>

#include "renderer/renderer_registry.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t spirv_magic = UINT32_C(0x07230203);
constexpr std::uint64_t maximum_shader_size = UINT64_C(64) * UINT64_C(1024) * UINT64_C(1024);
constexpr std::uint32_t maximum_entry_point_length = UINT32_C(255);

granit_result validate_desc(const granit_shader_desc* desc) noexcept {
  if (desc == nullptr || desc->struct_size < GRANIT_SHADER_DESC_VERSION_1_SIZE ||
      desc->code == nullptr || desc->code_size < sizeof(std::uint32_t) * 5 ||
      desc->code_size > maximum_shader_size || desc->code_size % sizeof(std::uint32_t) != 0 ||
      desc->entry_point == nullptr || desc->entry_point_length == 0 ||
      desc->entry_point_length > maximum_entry_point_length || desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (desc->stage < GRANIT_SHADER_STAGE_VERTEX || desc->stage > GRANIT_SHADER_STAGE_COMPUTE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::memchr(desc->entry_point, '\0', desc->entry_point_length) != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::uint32_t magic{};
  std::memcpy(&magic, desc->code, sizeof(magic));
  return magic == spirv_magic ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
}

} // namespace

extern "C" granit_result granit_shader_create(granit_renderer renderer,
                                              const granit_shader_desc* desc,
                                              granit_shader* shader) {
  if (shader == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *shader = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto validation = validate_desc(desc);
  if (validation != GRANIT_SUCCESS)
    return validation;
  try {
    std::vector<std::uint32_t> code(static_cast<std::size_t>(desc->code_size) /
                                    sizeof(std::uint32_t));
    std::memcpy(code.data(), desc->code, static_cast<std::size_t>(desc->code_size));
    return granit::detail::renderer_registry::instance().create_shader(
        renderer, desc->stage, code, std::string_view{desc->entry_point, desc->entry_point_length},
        *shader);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_shader_destroy(granit_renderer renderer, granit_shader shader) {
  if (renderer == GRANIT_NULL_HANDLE || shader == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().destroy_shader(renderer, shader);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
