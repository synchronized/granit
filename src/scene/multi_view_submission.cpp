// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "scene/multi_view_submission.h"

#include <limits>
#include <new>
#include <utility>

namespace granit::scene {
namespace {

bool too_many(std::size_t size) noexcept {
  return size > std::numeric_limits<std::uint32_t>::max();
}

template <typename Light, typename Predicate>
void collect_lights(std::span<const Light> lights, std::uint64_t view_mask,
                    std::vector<std::uint32_t>& output, Predicate&& predicate) {
  output.reserve(lights.size());
  for (std::size_t index = 0; index < lights.size(); ++index) {
    if ((view_mask & lights[index].layer_mask) != 0 && predicate(lights[index]))
      output.push_back(static_cast<std::uint32_t>(index));
  }
}

multi_view_error map_visibility_error(visibility_error error) noexcept {
  if (error == visibility_error::invalid_frustum)
    return multi_view_error::invalid_frustum;
  if (error == visibility_error::too_many_renderables)
    return multi_view_error::too_many_items;
  return error == visibility_error::out_of_memory ? multi_view_error::out_of_memory
                                                  : multi_view_error::none;
}

} // namespace

multi_view_error build_multi_view_snapshot(const multi_view_submission& submission,
                                           multi_view_snapshot& output) noexcept {
  if (submission.views.empty())
    return multi_view_error::empty_views;
  if (too_many(submission.views.size()) || too_many(submission.renderables.size()) ||
      too_many(submission.directional_lights.size()) || too_many(submission.point_lights.size()) ||
      too_many(submission.spot_lights.size())) {
    return multi_view_error::too_many_items;
  }

  try {
    multi_view_snapshot candidate;
    const frame_submission shared_submission{.view = submission.views.front(),
                                             .renderables = submission.renderables,
                                             .directional_lights = submission.directional_lights,
                                             .point_lights = submission.point_lights,
                                             .spot_lights = submission.spot_lights};
    if (build_frame_snapshot(shared_submission, candidate.scene_) != submission_error::none)
      return multi_view_error::invalid_submission;
    candidate.views_.reserve(submission.views.size());
    for (const auto& view : submission.views) {
      frame_snapshot validated_view;
      if (build_frame_snapshot({.view = view,
                                .renderables = {},
                                .directional_lights = {},
                                .point_lights = {},
                                .spot_lights = {}},
                               validated_view) != submission_error::none) {
        return multi_view_error::invalid_submission;
      }
      view_visibility result{.view = view,
                             .renderables = {},
                             .directional_lights = {},
                             .point_lights = {},
                             .spot_lights = {}};
      const auto visibility_result =
          build_visible_list(view, candidate.scene_.renderables(), result.renderables);
      if (visibility_result != visibility_error::none)
        return map_visibility_error(visibility_result);
      frustum view_frustum{};
      if (extract_frustum(view.view_projection, view_frustum) != visibility_error::none)
        return multi_view_error::invalid_frustum;
      collect_lights(candidate.scene_.directional_lights(), view.layer_mask,
                     result.directional_lights,
                     [](const directional_light_input&) { return true; });
      collect_lights(candidate.scene_.point_lights(), view.layer_mask, result.point_lights,
                     [&](const point_light_input& light) {
                       return intersects(view_frustum, {light.position, light.radius});
                     });
      collect_lights(candidate.scene_.spot_lights(), view.layer_mask, result.spot_lights,
                     [&](const spot_light_input& light) {
                       return intersects(view_frustum, {light.position, light.radius});
                     });
      candidate.views_.push_back(std::move(result));
    }
    output = std::move(candidate);
    return multi_view_error::none;
  } catch (const std::bad_alloc&) {
    return multi_view_error::out_of_memory;
  } catch (...) {
    return multi_view_error::out_of_memory;
  }
}

} // namespace granit::scene
