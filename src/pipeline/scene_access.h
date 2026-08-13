// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_SCENE_ACCESS_H
#define GRANIT_PIPELINE_SCENE_ACCESS_H

#include "scene/multi_view_submission.h"

#include <granit/core/result.h>
#include <granit/pipeline/scene.h>

namespace granit::pipeline::detail {

/** 为一次渲染复制快照；复制期间持有注册表锁，返回后不再依赖公共句柄。 */
[[nodiscard]] granit_result copy_scene_snapshot(granit_renderer renderer,
                                                granit_scene_snapshot snapshot,
                                                scene::multi_view_snapshot& output) noexcept;

} // namespace granit::pipeline::detail

#endif
