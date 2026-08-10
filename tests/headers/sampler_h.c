// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors
#include <granit/renderer/sampler.h>
typedef char granit_sampler_desc_size_check
    [(sizeof(granit_sampler_desc) == GRANIT_SAMPLER_DESC_VERSION_1_SIZE) ? 1 : -1];
granit_sampler granit_sampler_header_check(void) { return GRANIT_NULL_HANDLE; }
