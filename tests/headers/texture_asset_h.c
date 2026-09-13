// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
#include <granit/renderer/texture_asset.h>

typedef char granit_texture_content_id_size_check
    [sizeof(granit_texture_content_id) == sizeof(granit_asset_content_id) ? 1 : -1];
granit_texture_asset_info granit_texture_asset_header_check(void) {
  const granit_texture_asset_info value = GRANIT_TEXTURE_ASSET_INFO_INIT;
  return value;
}
