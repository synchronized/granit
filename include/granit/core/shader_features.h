// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_SHADER_FEATURES_H_
#define GRANIT_CORE_SHADER_FEATURES_H_

#include <stdint.h>

typedef uint64_t granit_shader_feature_flags;
#define GRANIT_SHADER_FEATURE_FLOAT16_BIT (UINT64_C(1) << 0)
#define GRANIT_SHADER_FEATURE_SUBGROUP_BIT (UINT64_C(1) << 1)
#define GRANIT_SHADER_FEATURE_ALL_BITS                                                             \
  (GRANIT_SHADER_FEATURE_FLOAT16_BIT | GRANIT_SHADER_FEATURE_SUBGROUP_BIT)

#define GRANIT_SHADER_PROFILE_PORTABLE UINT32_C(1)

#endif
