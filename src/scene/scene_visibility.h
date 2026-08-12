// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SCENE_SCENE_VISIBILITY_H
#define GRANIT_SCENE_SCENE_VISIBILITY_H

#include "scene/scene_submission.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace granit::scene {

struct frustum_plane {
  float3 normal{};
  float distance = 0.0F;
};

struct frustum {
  std::array<frustum_plane, 6> planes;
};

enum class visibility_error : std::uint8_t {
  none,
  invalid_frustum,
  too_many_renderables,
  out_of_memory,
};

class visible_list {
public:
  [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept { return indices_; }

private:
  friend visibility_error build_visible_list(const frame_snapshot&, visible_list&) noexcept;
  friend visibility_error build_visible_list(const view_input&, std::span<const renderable_input>,
                                             visible_list&) noexcept;

  std::vector<std::uint32_t> indices_;
};

/** 从列主序 view-projection 提取 Vulkan clip-space（z 为 [0, 1]）Frustum。 */
[[nodiscard]] visibility_error extract_frustum(const matrix4& view_projection,
                                               frustum& output) noexcept;

/** 球体与 Frustum 相交或位于内部时返回 true。 */
[[nodiscard]] bool intersects(const frustum& value, const bounding_sphere& bounds) noexcept;

/** 为单 View 快照构建按稳定键排序的可见 Renderable 索引。失败时不修改 output。 */
[[nodiscard]] visibility_error build_visible_list(const frame_snapshot& snapshot,
                                                  visible_list& output) noexcept;

/** 为显式 View 与 Renderable 数组构建可见列表。 */
[[nodiscard]] visibility_error build_visible_list(const view_input& view,
                                                  std::span<const renderable_input> renderables,
                                                  visible_list& output) noexcept;

} // namespace granit::scene

#endif
