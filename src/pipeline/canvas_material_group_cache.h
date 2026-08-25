// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_CANVAS_MATERIAL_GROUP_CACHE_H_
#define GRANIT_PIPELINE_CANVAS_MATERIAL_GROUP_CACHE_H_

#include <granit/pipeline/export.h>
#include <granit/pipeline/material.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/sampler.h>
#include <granit/renderer/texture.h>

namespace granit::pipeline::detail {

/** Canvas 专用 Material 绑定组有界缓存；键中的完整句柄包含资源 generation。 */
class GRANIT_RENDER_PIPELINE_API canvas_material_group_cache {
public:
  canvas_material_group_cache();
  ~canvas_material_group_cache();
  canvas_material_group_cache(const canvas_material_group_cache&) = delete;
  canvas_material_group_cache& operator=(const canvas_material_group_cache&) = delete;

  [[nodiscard]] granit_result acquire(granit_renderer renderer, granit_material material,
                                      granit_bind_group_layout layout, granit_texture_view texture,
                                      granit_sampler sampler, granit_bind_group& group);
  /** 在本帧全部绑定完成后回收到持久容量；Recorder 已保留本帧实际使用的资源。 */
  [[nodiscard]] granit_result trim() noexcept;
  [[nodiscard]] granit_result reset() noexcept;

private:
  struct impl;
  // 导出类只保存固定宽度实现指针，避免把 STL 布局带到 DLL 边界。
  impl* state_ = nullptr;
};

} // namespace granit::pipeline::detail

#endif
