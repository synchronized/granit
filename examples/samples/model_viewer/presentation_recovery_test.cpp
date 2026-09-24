// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/presentation_recovery.h"

#include <catch2/catch_all.hpp>

namespace viewer = granit::example::model_viewer;

TEST_CASE("显示结果映射到跨平台恢复策略", "[sample][model-viewer][presentation]") {
  const auto rendered = viewer::classify_presentation_result(granit::result::success);
  CHECK(rendered.action == viewer::presentation_action::proceed);
  CHECK(rendered.frame_rendered);

  const auto recreate = viewer::classify_presentation_result(granit::result::success, true);
  CHECK(recreate.action == viewer::presentation_action::recreate_swapchain);
  CHECK(recreate.frame_rendered);

  CHECK(viewer::classify_presentation_result(granit::result::not_ready).action ==
        viewer::presentation_action::retry);
  CHECK(viewer::classify_presentation_result(granit::result::out_of_date).action ==
        viewer::presentation_action::recreate_swapchain);
  CHECK(viewer::classify_presentation_result(granit::result::surface_lost).action ==
        viewer::presentation_action::recreate_surface);
  CHECK(viewer::classify_presentation_result(granit::result::device_lost).action ==
        viewer::presentation_action::stop);
  CHECK(viewer::classify_presentation_result(granit::result::invalid_argument).action ==
        viewer::presentation_action::stop);
}
