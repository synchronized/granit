// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader.h>

typedef char
    granit_shader_desc_size_check[sizeof(granit_shader_desc) >= GRANIT_SHADER_DESC_SIZE ? 1 : -1];
typedef char
    granit_shader_digest_size_check[sizeof(granit_shader_digest) == GRANIT_SHADER_DIGEST_SIZE ? 1
                                                                                              : -1];
typedef char granit_shader_content_id_size_check
    [sizeof(granit_shader_content_id) == sizeof(granit_shader_digest) ? 1 : -1];
typedef char granit_shader_cache_key_size_check
    [sizeof(granit_shader_cache_key) == sizeof(granit_shader_digest) ? 1 : -1];
typedef char granit_shader_asset_info_size_check
    [sizeof(granit_shader_asset_info) == GRANIT_SHADER_ASSET_INFO_SIZE ? 1 : -1];

granit_shader_desc granit_shader_header_check(void) {
  const granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
  return desc;
}

granit_shader_asset_info granit_shader_asset_info_header_check(void) {
  const granit_shader_asset_info info = GRANIT_SHADER_ASSET_INFO_INIT;
  return info;
}
