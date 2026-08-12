// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SCENE_MULTI_VIEW_SUBMISSION_H
#define GRANIT_SCENE_MULTI_VIEW_SUBMISSION_H

#include "scene/scene_submission.h"
#include "scene/scene_visibility.h"

#include <cstdint>
#include <span>
#include <vector>

namespace granit::scene {

struct multi_view_submission {
  std::span<const view_input> views;
  std::span<const renderable_input> renderables;
  std::span<const directional_light_input> directional_lights;
  std::span<const point_light_input> point_lights;
  std::span<const spot_light_input> spot_lights;
};

struct view_visibility {
  view_input view;
  visible_list renderables;
  std::vector<std::uint32_t> directional_lights;
  std::vector<std::uint32_t> point_lights;
  std::vector<std::uint32_t> spot_lights;
};

enum class multi_view_error : std::uint8_t {
  none,
  empty_views,
  invalid_submission,
  invalid_frustum,
  too_many_items,
  out_of_memory,
};

class multi_view_snapshot {
public:
  [[nodiscard]] std::span<const renderable_input> renderables() const noexcept {
    return scene_.renderables();
  }
  [[nodiscard]] std::span<const directional_light_input> directional_lights() const noexcept {
    return scene_.directional_lights();
  }
  [[nodiscard]] std::span<const point_light_input> point_lights() const noexcept {
    return scene_.point_lights();
  }
  [[nodiscard]] std::span<const spot_light_input> spot_lights() const noexcept {
    return scene_.spot_lights();
  }
  [[nodiscard]] std::span<const view_visibility> views() const noexcept { return views_; }

private:
  friend multi_view_error build_multi_view_snapshot(const multi_view_submission&,
                                                    multi_view_snapshot&) noexcept;

  frame_snapshot scene_;
  std::vector<view_visibility> views_;
};

/** 校验并复制共享场景数据，为每个 View 构建独立可见索引；失败时不修改 output。 */
[[nodiscard]] multi_view_error build_multi_view_snapshot(const multi_view_submission& submission,
                                                         multi_view_snapshot& output) noexcept;

} // namespace granit::scene

#endif
