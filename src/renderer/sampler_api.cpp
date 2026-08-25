// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/resource_validation.h"
#include "renderer/renderer_registry.h"
#include <granit/renderer/sampler.h>

extern "C" granit_result granit_sampler_create(granit_renderer renderer,
                                               const granit_sampler_desc* desc,
                                               granit_sampler* sampler) {
  if (desc == nullptr || sampler == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *sampler = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto result = granit::detail::validate_sampler_desc(*desc);
  if (result != GRANIT_SUCCESS)
    return result;
  return granit::detail::renderer_registry::instance().create_sampler(renderer, *desc, *sampler);
}

extern "C" granit_result granit_sampler_destroy(granit_renderer renderer, granit_sampler sampler) {
  if (renderer == GRANIT_NULL_HANDLE || sampler == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().destroy_sampler(renderer, sampler);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
