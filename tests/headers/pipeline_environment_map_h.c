// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/environment_map.h>

typedef char granit_environment_map_asset_desc_v1_size
    [sizeof(granit_environment_map_asset_desc) == GRANIT_ENVIRONMENT_MAP_ASSET_DESC_VERSION_1_SIZE
         ? 1
         : -1];
typedef char granit_environment_map_info_v1_size
    [sizeof(granit_environment_map_info) == GRANIT_ENVIRONMENT_MAP_INFO_VERSION_1_SIZE ? 1 : -1];
