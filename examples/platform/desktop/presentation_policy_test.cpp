// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "presentation_policy.h"

#include <catch2/catch_all.hpp>

namespace desktop = granit::example::model_viewer::desktop;

TEST_CASE("桌面显示结果映射到明确恢复策略", "[example][model-viewer][desktop]") {
  CHECK(desktop::classify_presentation_result(granit::result::success) ==
        desktop::presentation_action::proceed);
  CHECK(desktop::classify_presentation_result(granit::result::not_ready) ==
        desktop::presentation_action::retry);
  CHECK(desktop::classify_presentation_result(granit::result::out_of_date) ==
        desktop::presentation_action::recreate_swapchain);
  CHECK(desktop::classify_presentation_result(granit::result::surface_lost) ==
        desktop::presentation_action::recreate_surface);
  CHECK(desktop::classify_presentation_result(granit::result::device_lost) ==
        desktop::presentation_action::stop);
  CHECK(desktop::classify_presentation_result(granit::result::invalid_argument) ==
        desktop::presentation_action::stop);
}
