// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/resource_types.h>

typedef char granit_buffer_desc_size_check
    [(sizeof(granit_buffer_desc) == GRANIT_BUFFER_DESC_VERSION_1_SIZE) ? 1 : -1];
typedef char granit_texture_desc_size_check
    [(sizeof(granit_texture_desc) == GRANIT_TEXTURE_DESC_VERSION_1_SIZE) ? 1 : -1];
typedef char granit_texture_view_desc_size_check
    [(sizeof(granit_texture_view_desc) == GRANIT_TEXTURE_VIEW_DESC_VERSION_1_SIZE) ? 1 : -1];
typedef char granit_sampler_desc_size_check
    [(sizeof(granit_sampler_desc) == GRANIT_SAMPLER_DESC_VERSION_1_SIZE) ? 1 : -1];

granit_buffer_usage granit_resource_types_header_check(void) {
  return GRANIT_BUFFER_USAGE_VERTEX_BIT | GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
}
