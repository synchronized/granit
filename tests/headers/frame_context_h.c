// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/frame_context.h>

typedef char granit_frame_context_desc_size_check
    [sizeof(granit_frame_context_desc) == GRANIT_FRAME_CONTEXT_DESC_VERSION_1_SIZE ? 1 : -1];
granit_frame_context_desc granit_frame_context_h_header_test(void) {
  granit_result (*get_frame_info)(granit_renderer, granit_frame, granit_frame_info*) =
      granit_frame_get_info;
  const granit_frame_context_desc desc = GRANIT_FRAME_CONTEXT_DESC_INIT;
  (void)get_frame_info;
  return desc;
}
