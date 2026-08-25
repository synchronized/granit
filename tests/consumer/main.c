// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.h>

#include "linkage_check.h"

#include <stdio.h>
#include <string.h>

int main(void) {
  char header_version[32] = {0};
  (void)snprintf(header_version, sizeof(header_version), "%u.%u.%u", GRANIT_VERSION_MAJOR,
                 GRANIT_VERSION_MINOR, GRANIT_VERSION_PATCH);
  if (strcmp(header_version, GRANIT_CONSUMER_PACKAGE_VERSION) != 0)
    return 1;
  if (granit_version_major() != GRANIT_VERSION_MAJOR ||
      granit_version_minor() != GRANIT_VERSION_MINOR ||
      granit_version_patch() != GRANIT_VERSION_PATCH)
    return 2;

  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_renderer_desc invalid_desc = GRANIT_RENDERER_DESC_INIT;
  invalid_desc.struct_size = 0;
  if (granit_renderer_create(&invalid_desc, &renderer) != GRANIT_ERROR_INVALID_ARGUMENT ||
      renderer != GRANIT_NULL_HANDLE)
    return 3;

  const granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  const granit_result renderer_result = granit_renderer_create(&renderer_desc, &renderer);
  if (renderer_result == GRANIT_ERROR_BACKEND_UNAVAILABLE ||
      renderer_result == GRANIT_ERROR_INCOMPATIBLE_DRIVER ||
      renderer_result == GRANIT_ERROR_NO_SUITABLE_DEVICE)
    return renderer == GRANIT_NULL_HANDLE ? 0 : 4;
  if (renderer_result != GRANIT_SUCCESS || renderer == GRANIT_NULL_HANDLE)
    return 5;

  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  buffer_desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT;
  buffer_desc.memory_location = GRANIT_MEMORY_LOCATION_UPLOAD;
  buffer_desc.size = UINT64_C(64);
  granit_buffer buffer = GRANIT_NULL_HANDLE;
  if (granit_buffer_create(renderer, &buffer_desc, &buffer) != GRANIT_SUCCESS ||
      buffer == GRANIT_NULL_HANDLE)
    return 6;
  if (granit_buffer_destroy(renderer, buffer) != GRANIT_SUCCESS)
    return 7;
  if (granit_buffer_destroy(renderer, buffer) != GRANIT_ERROR_INVALID_HANDLE)
    return 8;
  if (granit_renderer_destroy(renderer) != GRANIT_SUCCESS)
    return 9;
  return granit_renderer_destroy(renderer) == GRANIT_ERROR_INVALID_HANDLE ? 0 : 10;
}
