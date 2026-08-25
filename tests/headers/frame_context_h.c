// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/frame_context.h>

typedef char granit_frame_context_desc_size_check
    [sizeof(granit_frame_context_desc) == GRANIT_FRAME_CONTEXT_DESC_VERSION_1_SIZE ? 1 : -1];

granit_frame_context_desc granit_frame_context_h_header_test(void) {
  const granit_frame_context_desc desc = GRANIT_FRAME_CONTEXT_DESC_INIT;
  return desc;
}
