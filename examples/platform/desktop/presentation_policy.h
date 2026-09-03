// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLE_MODEL_VIEWER_PRESENTATION_POLICY_H_
#define GRANIT_EXAMPLE_MODEL_VIEWER_PRESENTATION_POLICY_H_

#include <granit/core/result.hpp>

namespace granit::example::model_viewer::desktop {

enum class presentation_action {
  proceed,
  retry,
  recreate_swapchain,
  recreate_surface,
  stop,
};

[[nodiscard]] constexpr presentation_action classify_presentation_result(result value) noexcept {
  if (value == result::success)
    return presentation_action::proceed;
  if (value == result::not_ready)
    return presentation_action::retry;
  if (value == result::out_of_date)
    return presentation_action::recreate_swapchain;
  if (value == result::surface_lost)
    return presentation_action::recreate_surface;
  return presentation_action::stop;
}

} // namespace granit::example::model_viewer::desktop

#endif
