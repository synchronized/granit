// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/core/content_id.h>

typedef char granit_content_digest_size_check
    [sizeof(granit_content_digest) == GRANIT_CONTENT_DIGEST_SIZE ? 1 : -1];
typedef char granit_asset_content_id_size_check
    [sizeof(granit_asset_content_id) == sizeof(granit_content_digest) ? 1 : -1];

granit_asset_content_id* granit_content_id_header_check(void) {
  return 0;
}
