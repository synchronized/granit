// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.h>

#include "../abi/snapshots/0.1.0/optional_components_identity.h"

typedef char granit_material_desc_v1_size
    [sizeof(granit_material_desc) == GRANIT_MATERIAL_DESC_VERSION_1_SIZE ? 1 : -1];
