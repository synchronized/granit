// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/text_draw_list.h>

#include "../abi/snapshots/0.1.0/optional_components_identity.h"

typedef char granit_text_run_v1_size
    [sizeof(granit_text_glyph_run_desc) == GRANIT_TEXT_GLYPH_RUN_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_text_list_v1_size
    [sizeof(granit_text_draw_list_desc) == GRANIT_TEXT_DRAW_LIST_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_text_stats_v1_size
    [sizeof(granit_text_draw_list_stats) == GRANIT_TEXT_DRAW_LIST_STATS_VERSION_1_SIZE ? 1 : -1];

static granit_text_draw_list_desc desc = GRANIT_TEXT_DRAW_LIST_DESC_INIT;
static granit_text_glyph_run_desc run = GRANIT_TEXT_GLYPH_RUN_DESC_INIT;
static granit_text_draw_list_stats stats = GRANIT_TEXT_DRAW_LIST_STATS_INIT;

void granit_pipeline_text_draw_list_h_compile(void) {
  (void)desc;
  (void)run;
  (void)stats;
}
