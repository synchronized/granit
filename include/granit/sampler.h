// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SAMPLER_H_
#define GRANIT_SAMPLER_H_

#include <granit/export.h>
#include <granit/renderer.h>
#include <granit/resource_types.h>
#include <granit/result.h>
#include <granit/types.h>

/** 独立采样状态句柄。零值无效。 */
typedef granit_handle granit_sampler;

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API granit_result granit_sampler_create(granit_renderer renderer,
                                               const granit_sampler_desc* desc,
                                               granit_sampler* sampler);
GRANIT_API granit_result granit_sampler_destroy(granit_renderer renderer, granit_sampler sampler);

#ifdef __cplusplus
}
#endif
#endif
