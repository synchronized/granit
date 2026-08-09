// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/render_target.h>

typedef char granit_color_attachment_size_check
    [sizeof(granit_color_attachment_desc) == GRANIT_COLOR_ATTACHMENT_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_depth_stencil_attachment_size_check
    [sizeof(granit_depth_stencil_attachment_desc) ==
             GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_VERSION_1_SIZE
         ? 1
         : -1];
typedef char granit_rendering_desc_size_check
    [sizeof(granit_rendering_desc) == GRANIT_RENDERING_DESC_VERSION_1_SIZE ? 1 : -1];

granit_color_attachment_desc granit_render_target_c_header_check(void) {
  const granit_color_attachment_desc desc = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  return desc;
}
