// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/canvas_draw_list.h>

#include "../abi/snapshots/0.1.0/optional_components_identity.h"

typedef char granit_canvas_desc_v1_size
    [sizeof(granit_canvas_draw_list_desc) == GRANIT_CANVAS_DRAW_LIST_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_canvas_rect_v1_size
    [sizeof(granit_canvas_rect_desc) == GRANIT_CANVAS_RECT_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_canvas_stats_v1_size[sizeof(granit_canvas_draw_list_stats) ==
                                                 GRANIT_CANVAS_DRAW_LIST_STATS_VERSION_1_SIZE
                                             ? 1
                                             : -1];
typedef char granit_canvas_record_v1_size
    [sizeof(granit_canvas_record_desc) >= GRANIT_CANVAS_RECORD_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_canvas_record_v2_size
    [sizeof(granit_canvas_record_desc) == GRANIT_CANVAS_RECORD_DESC_VERSION_2_SIZE ? 1 : -1];

static granit_canvas_draw_list_desc list_desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
static granit_canvas_rect_desc rect_desc = GRANIT_CANVAS_RECT_DESC_INIT;
static granit_canvas_draw_list_stats list_stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
static granit_canvas_draw_range draw_range;

void granit_pipeline_canvas_draw_list_h_compile(void) {
  (void)list_desc;
  (void)rect_desc;
  (void)list_stats;
  (void)draw_range;
  (void)&granit_canvas_draw_list_append_batch;
}
