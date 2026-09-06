// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.h>

#include "../abi/snapshots/0.1.0/optional_components_identity.h"

typedef char granit_material_desc_v1_size
    [sizeof(granit_material_desc) == GRANIT_MATERIAL_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_material_pipeline_warmup_desc_size
    [sizeof(granit_material_pipeline_warmup_desc) == 40 ? 1 : -1];

static const granit_material_pipeline_warmup_desc granit_material_warmup_default =
    GRANIT_MATERIAL_PIPELINE_WARMUP_DESC_INIT;

void granit_pipeline_material_h_compiles(void) {
  (void)granit_material_warmup_default;
}
