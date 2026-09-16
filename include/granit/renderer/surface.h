// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SURFACE_H_
#define GRANIT_SURFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/renderer.h>

/** 窗口输出 Surface 句柄。零值无效。 */
typedef granit_handle granit_surface;

#ifdef __cplusplus
extern "C" {
#endif

/** 销毁属于指定 Renderer 的 Surface，并使句柄立即失效。 */
GRANIT_API granit_result granit_surface_destroy(granit_renderer renderer, granit_surface surface);

#ifdef __cplusplus
}
#endif

#endif
