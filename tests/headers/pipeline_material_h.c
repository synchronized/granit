// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.h>

typedef char granit_material_desc_v1_size
    [sizeof(granit_material_desc) == GRANIT_MATERIAL_DESC_VERSION_1_SIZE ? 1 : -1];
