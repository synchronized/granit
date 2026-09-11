// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/core/shader_types.h>

typedef char granit_shader_content_id_size_check
    [sizeof(granit_shader_content_id) == sizeof(granit_asset_content_id) ? 1 : -1];
typedef char granit_shader_cache_key_size_check
    [sizeof(granit_shader_cache_key) == sizeof(granit_content_digest) ? 1 : -1];

granit_shader_stage granit_shader_types_header_check(void) {
  return GRANIT_SHADER_STAGE_VERTEX;
}
