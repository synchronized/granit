// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.h>

typedef char granit_renderer_desc_size_check
    [sizeof(granit_renderer_desc) >= GRANIT_RENDERER_DESC_VERSION_3_SIZE ? 1 : -1];

granit_renderer_desc granit_renderer_header_check(void) {
  const granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  return desc;
}
