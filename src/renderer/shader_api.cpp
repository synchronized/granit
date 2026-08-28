// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader.h>

#include "renderer/renderer_registry.h"
#include "renderer/shader_validation.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string_view>
#include <vector>

extern "C" granit_result granit_shader_create(granit_renderer renderer,
                                              const granit_shader_desc* desc,
                                              granit_shader* shader) {
  if (shader == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *shader = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto validation =
#ifdef __EMSCRIPTEN__
      granit::detail::validate_shader_wgsl(desc);
#else
      granit::detail::validate_shader_spirv(desc);
#endif
  if (validation != GRANIT_SUCCESS)
    return validation;
  try {
#ifdef __EMSCRIPTEN__
    return granit::detail::renderer_registry::instance().create_wgsl_shader(
        renderer, desc->stage, {desc->wgsl, static_cast<std::size_t>(desc->wgsl_length)},
        {desc->entry_point, static_cast<std::size_t>(desc->entry_point_length)}, *shader);
#else
    std::vector<std::uint32_t> code(static_cast<std::size_t>(desc->code_size) /
                                    sizeof(std::uint32_t));
    std::memcpy(code.data(), desc->code, static_cast<std::size_t>(desc->code_size));
    return granit::detail::renderer_registry::instance().create_shader(
        renderer, desc->stage, code, std::string_view{desc->entry_point, desc->entry_point_length},
        *shader);
#endif
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
