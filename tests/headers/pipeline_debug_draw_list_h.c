// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/debug_draw_list.h>

#include "../abi/snapshots/0.1.0/optional_components_identity.h"

typedef char granit_debug_list_v1_size
    [sizeof(granit_debug_draw_list_desc) == GRANIT_DEBUG_DRAW_LIST_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_debug_stats_v1_size
    [sizeof(granit_debug_draw_list_stats) == GRANIT_DEBUG_DRAW_LIST_STATS_VERSION_1_SIZE ? 1 : -1];
typedef char granit_debug_record_v1_size[sizeof(granit_debug_draw_record_desc) ==
                                                 GRANIT_DEBUG_DRAW_RECORD_DESC_VERSION_1_SIZE
                                             ? 1
                                             : -1];

static granit_debug_draw_list_desc desc = GRANIT_DEBUG_DRAW_LIST_DESC_INIT;
static granit_debug_draw_list_stats stats = GRANIT_DEBUG_DRAW_LIST_STATS_INIT;
static granit_debug_draw_record_desc record_desc = GRANIT_DEBUG_DRAW_RECORD_DESC_INIT;

void granit_pipeline_debug_draw_list_h_compile(void) {
  (void)desc;
  (void)stats;
  (void)record_desc;
}
