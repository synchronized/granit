// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/timestamp_query.h>

typedef char granit_timestamp_query_desc_size_check
    [sizeof(granit_timestamp_query_pool_desc) == GRANIT_TIMESTAMP_QUERY_POOL_DESC_VERSION_1_SIZE
         ? 1
         : -1];
