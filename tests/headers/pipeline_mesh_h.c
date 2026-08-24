// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/mesh.h>

#include "../abi/snapshots/0.1.0/optional_components_identity.h"

typedef char
    granit_mesh_desc_v1_size[sizeof(granit_mesh_desc) == GRANIT_MESH_DESC_VERSION_1_SIZE ? 1 : -1];

void granit_pipeline_mesh_h_compiles(void) {
  granit_mesh_desc desc = GRANIT_MESH_DESC_INIT;
  (void)desc;
}
