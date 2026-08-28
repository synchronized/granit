// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader.h>

#include "renderer/shader_validation.h"
#include "renderer_registry.h"

extern "C" granit_result granit_shader_create(granit_renderer renderer,
                                              const granit_shader_desc* desc,
                                              granit_shader* shader) {
  if (shader == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *shader = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto validation = granit::detail::validate_shader_wgsl(desc);
  if (validation != GRANIT_SUCCESS) {
    return validation;
  }
  return granit::detail::web_renderer_registry::instance().create_shader(
      renderer, desc->stage, desc->wgsl, desc->wgsl_length, desc->entry_point,
      desc->entry_point_length, *shader);
}

extern "C" granit_result granit_shader_destroy(granit_renderer renderer, granit_shader shader) {
  if (renderer == GRANIT_NULL_HANDLE || shader == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return granit::detail::web_renderer_registry::instance().destroy_shader(renderer, shader);
}
