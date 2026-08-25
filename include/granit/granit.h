// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_GRANIT_H_
#define GRANIT_GRANIT_H_

#include <stdint.h>

#include <granit/core/diagnostic.h>
#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/core/version.h>
#include <granit/math/types.h>
#include <granit/renderer/buffer.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/frame_context.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/render_target.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/resource_types.h>
#include <granit/renderer/sampler.h>
#include <granit/renderer/shader.h>
#include <granit/renderer/surface.h>
#include <granit/renderer/swapchain.h>
#include <granit/renderer/texture.h>
#include <granit/renderer/timestamp_query.h>
#include <granit/renderer/upload_batch.h>

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
