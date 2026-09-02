// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/screenshot_comparison.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace model_viewer = granit::example::model_viewer;

TEST_CASE("截图比较允许一像素轮廓偏移", "[example][model-viewer][screenshot]") {
  std::array<std::uint8_t, 8 * 8 * 4> expected{};
  std::array<std::uint8_t, 8 * 8 * 4> actual{};
  for (std::size_t pixel = 0; pixel < 64; ++pixel) {
    expected[pixel * 4 + 3] = 255;
    actual[pixel * 4 + 3] = 255;
  }
  for (std::size_t y = 2; y < 6; ++y) {
    for (std::size_t x = 2; x < 5; ++x) {
      const auto expected_offset = (y * 8 + x) * 4;
      const auto actual_offset = (y * 8 + x + 1) * 4;
      for (std::size_t channel = 0; channel < 3; ++channel) {
        expected[expected_offset + channel] = static_cast<std::uint8_t>(200 - channel * 50);
        actual[actual_offset + channel] = static_cast<std::uint8_t>(200 - channel * 50);
      }
    }
  }
  model_viewer::screenshot_comparison_report report;
  REQUIRE(model_viewer::compare_screenshots({8, 8, expected, {}}, {8, 8, actual, {}}, {}, report) ==
          model_viewer::screenshot_comparison_error::none);
  INFO("silhouette=" << report.silhouette_mismatch_count
                     << ", colors=" << report.compared_color_pixel_count
                     << ", mae=" << report.color_mean_absolute_error
                     << ", outliers=" << report.color_outlier_ratio);
  CHECK(report.passed);
  CHECK(report.silhouette_mismatch_count == 0);
}

TEST_CASE("截图比较报告颜色和深度差异", "[example][model-viewer][screenshot]") {
  std::array<std::uint8_t, 5 * 5 * 4> expected{};
  std::array<std::uint8_t, 5 * 5 * 4> actual{};
  for (std::size_t pixel = 0; pixel < 25; ++pixel) {
    expected[pixel * 4 + 3] = 255;
    actual[pixel * 4 + 3] = 255;
  }
  for (std::size_t y = 1; y < 4; ++y) {
    for (std::size_t x = 1; x < 4; ++x) {
      const auto offset = (y * 5 + x) * 4;
      expected[offset] = actual[offset] = 100;
      expected[offset + 1] = actual[offset + 1] = 100;
      expected[offset + 2] = actual[offset + 2] = 100;
    }
  }
  actual[(2 * 5 + 2) * 4] = 200;
  std::array<float, 25> expected_depth{};
  std::array<float, 25> actual_depth{};
  expected_depth.fill(1.0F);
  actual_depth.fill(1.0F);
  expected_depth[2 * 5 + 2] = 0.25F;
  actual_depth[2 * 5 + 2] = 0.75F;

  model_viewer::screenshot_comparison_report report;
  REQUIRE(model_viewer::compare_screenshots({5, 5, expected, expected_depth},
                                            {5, 5, actual, actual_depth}, {}, report) ==
          model_viewer::screenshot_comparison_error::none);
  CHECK_FALSE(report.passed);
  CHECK(report.color_outlier_count == 1);
  CHECK(report.depth_outlier_count == 1);
}

TEST_CASE("截图比较参数错误时保留旧报告", "[example][model-viewer][screenshot]") {
  constexpr std::array<std::uint8_t, 4> pixel{0, 0, 0, 255};
  model_viewer::screenshot_comparison_report report{.passed = true};
  auto options = model_viewer::screenshot_comparison_options{};
  options.max_color_outlier_ratio = 2.0;
  CHECK(model_viewer::compare_screenshots({1, 1, pixel, {}}, {1, 1, pixel, {}}, options, report) ==
        model_viewer::screenshot_comparison_error::invalid_options);
  CHECK(report.passed);
}
