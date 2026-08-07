// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>

int main(void) {
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  renderer_desc.api_version = 0;

  return sizeof(granit_handle) == sizeof(uint64_t) && GRANIT_NULL_HANDLE == 0 &&
             granit_result_message(GRANIT_SUCCESS) != 0 &&
             granit_result_message(GRANIT_ERROR_INVALID_ARGUMENT) != 0 &&
             granit_renderer_create(0, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT &&
             granit_renderer_create(&renderer_desc, &renderer) == GRANIT_ERROR_INVALID_ARGUMENT &&
             granit_renderer_destroy(GRANIT_NULL_HANDLE) == GRANIT_ERROR_INVALID_HANDLE &&
             granit_version_major() == GRANIT_VERSION_MAJOR &&
             granit_version_minor() == GRANIT_VERSION_MINOR &&
             granit_version_patch() == GRANIT_VERSION_PATCH
           ? 0
           : 1;
}
