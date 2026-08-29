// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.h>

typedef char granit_renderer_desc_size_check
    [sizeof(granit_renderer_desc) >= GRANIT_RENDERER_DESC_VERSION_3_SIZE ? 1 : -1];
typedef char granit_renderer_limits_size_check
    [sizeof(granit_renderer_limits) == GRANIT_RENDERER_LIMITS_VERSION_1_SIZE ? 1 : -1];
typedef char granit_renderer_resource_stats_size_check
    [sizeof(granit_renderer_resource_stats) == GRANIT_RENDERER_RESOURCE_STATS_VERSION_1_SIZE ? 1
                                                                                             : -1];

granit_renderer_desc granit_renderer_header_check(void) {
  const granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  return desc;
}

granit_renderer_limits granit_renderer_limits_header_check(void) {
  const granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
  return limits;
}

granit_renderer_resource_stats granit_renderer_resource_stats_header_check(void) {
  const granit_renderer_resource_stats stats = GRANIT_RENDERER_RESOURCE_STATS_INIT;
  return stats;
}
