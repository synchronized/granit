// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_CANVAS_MATERIAL_GROUP_CACHE_H_
#define GRANIT_PIPELINE_CANVAS_MATERIAL_GROUP_CACHE_H_

#include <granit/pipeline/export.h>
#include <granit/pipeline/material.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/sampler.h>
#include <granit/renderer/texture.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace granit::pipeline::detail {

/** Canvas 专用 Material 绑定组有界缓存；键中的完整句柄包含资源 generation。 */
class GRANIT_RENDER_PIPELINE_API canvas_material_group_cache {
public:
  canvas_material_group_cache() = default;
  ~canvas_material_group_cache();
  canvas_material_group_cache(const canvas_material_group_cache&) = delete;
  canvas_material_group_cache& operator=(const canvas_material_group_cache&) = delete;

  [[nodiscard]] granit_result acquire(granit_renderer renderer, granit_material material,
                                      granit_bind_group_layout layout, granit_texture_view texture,
                                      granit_sampler sampler, granit_bind_group& group);
  [[nodiscard]] granit_result reset() noexcept;

private:
  struct entry {
    granit_texture_view texture = GRANIT_NULL_HANDLE;
    granit_sampler sampler = GRANIT_NULL_HANDLE;
    granit_bind_group group = GRANIT_NULL_HANDLE;
    std::uint64_t last_use = 0;
  };

  static constexpr std::size_t capacity = 64;
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_material material_ = GRANIT_NULL_HANDLE;
  granit_bind_group_layout layout_ = GRANIT_NULL_HANDLE;
  std::uint64_t use_serial_ = 0;
  std::vector<entry> entries_;
};

} // namespace granit::pipeline::detail

#endif
