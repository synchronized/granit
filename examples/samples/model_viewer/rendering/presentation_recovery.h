// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_PRESENTATION_RECOVERY_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_PRESENTATION_RECOVERY_H_

#include <granit/core/result.hpp>

namespace granit::example::model_viewer {

enum class presentation_action {
  proceed,
  retry,
  recreate_swapchain,
  recreate_surface,
  stop,
};

struct presentation_outcome {
  presentation_action action{presentation_action::stop};
  bool frame_rendered{};
};

/** 将同步或异步 Present 结果转换为平台壳执行的恢复动作。 */
[[nodiscard]] constexpr presentation_outcome
classify_presentation_result(result value, bool needs_recreate = false) noexcept {
  if (value == result::success) {
    return {.action = needs_recreate ? presentation_action::recreate_swapchain
                                     : presentation_action::proceed,
            .frame_rendered = true};
  }
  if (value == result::not_ready)
    return {.action = presentation_action::retry};
  if (value == result::out_of_date)
    return {.action = presentation_action::recreate_swapchain};
  if (value == result::surface_lost)
    return {.action = presentation_action::recreate_surface};
  return {.action = presentation_action::stop};
}

} // namespace granit::example::model_viewer

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_PRESENTATION_RECOVERY_H_
