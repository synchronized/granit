// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/math/types.h>

typedef char granit_float2_size_must_be_8[(sizeof(granit_float2) == 8) ? 1 : -1];
typedef char granit_float3_size_must_be_12[(sizeof(granit_float3) == 12) ? 1 : -1];
typedef char granit_float4_size_must_be_16[(sizeof(granit_float4) == 16) ? 1 : -1];
typedef char granit_matrix4_size_must_be_64[(sizeof(granit_matrix4) == 64) ? 1 : -1];
