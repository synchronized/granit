// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/text_atlas.h>

typedef char granit_text_atlas_v1_size
    [sizeof(granit_text_atlas_desc) == GRANIT_TEXT_ATLAS_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_text_bitmap_v1_size[sizeof(granit_text_glyph_bitmap_desc) ==
                                                GRANIT_TEXT_GLYPH_BITMAP_DESC_VERSION_1_SIZE
                                            ? 1
                                            : -1];
typedef char granit_text_atlas_stats_v1_size
    [sizeof(granit_text_atlas_stats) == GRANIT_TEXT_ATLAS_STATS_VERSION_1_SIZE ? 1 : -1];

static granit_text_atlas_desc desc = GRANIT_TEXT_ATLAS_DESC_INIT;
static granit_text_glyph_bitmap_desc glyph = GRANIT_TEXT_GLYPH_BITMAP_DESC_INIT;
static granit_text_atlas_stats stats = GRANIT_TEXT_ATLAS_STATS_INIT;

void granit_pipeline_text_atlas_h_compile(void) {
  (void)desc;
  (void)glyph;
  (void)stats;
}
