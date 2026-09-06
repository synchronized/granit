// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/pbr_material.h>

typedef char
    granit_pbr_constant_buffer_size_is_stable[GRANIT_PBR_CONSTANT_BUFFER_SIZE == UINT32_C(48) ? 1
                                                                                              : -1];
