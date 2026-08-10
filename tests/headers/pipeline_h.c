// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline.h>

typedef char granit_pipeline_layout_desc_size_check
    [sizeof(granit_pipeline_layout_desc) >= GRANIT_PIPELINE_LAYOUT_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_graphics_pipeline_desc_size_check
    [sizeof(granit_graphics_pipeline_desc) >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_1_SIZE ? 1
                                                                                           : -1];
typedef char granit_bind_group_layout_desc_size_check
    [sizeof(granit_bind_group_layout_desc) >= GRANIT_BIND_GROUP_LAYOUT_DESC_VERSION_1_SIZE ? 1
                                                                                           : -1];
typedef char granit_bind_group_desc_size_check
    [sizeof(granit_bind_group_desc) >= GRANIT_BIND_GROUP_DESC_VERSION_1_SIZE ? 1 : -1];

granit_pipeline_layout_desc granit_pipeline_header_check(void) {
  const granit_pipeline_layout_desc desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
  return desc;
}
