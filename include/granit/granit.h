// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_GRANIT_H_
#define GRANIT_GRANIT_H_

#include <stdint.h>

#include <granit/export.h>
#include <granit/renderer.h>
#include <granit/result.h>
#include <granit/surface.h>
#include <granit/swapchain.h>
#include <granit/types.h>
#include <granit/version.h>

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API uint32_t granit_version_major(void);
GRANIT_API uint32_t granit_version_minor(void);
GRANIT_API uint32_t granit_version_patch(void);

#ifdef __cplusplus
}
#endif

#endif
