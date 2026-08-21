// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/scene.h>

typedef char granit_scene_desc_v1_size
    [sizeof(granit_scene_snapshot_desc) == GRANIT_SCENE_SNAPSHOT_DESC_VERSION_1_SIZE ? 1 : -1];
