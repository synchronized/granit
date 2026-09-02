// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_SCREENSHOT_COMPARISON_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_SCREENSHOT_COMPARISON_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace granit::example::model_viewer {

struct screenshot_view {
  std::uint32_t width{};
  std::uint32_t height{};
  std::span<const std::uint8_t> rgba;
  std::span<const float> depth;
};

struct screenshot_comparison_options {
  std::array<std::uint8_t, 4> background{0, 0, 0, 255};
  std::uint8_t background_tolerance{4};
  std::uint8_t edge_tolerance_pixels{1};
  std::size_t max_silhouette_mismatch_count{};
  std::uint8_t color_channel_threshold{12};
  double max_color_mean_absolute_error{3.0};
  double max_color_outlier_ratio{0.01};
  float depth_absolute_tolerance{0.002F};
  double max_depth_outlier_ratio{0.01};
};

struct screenshot_comparison_report {
  bool passed{};
  std::size_t silhouette_mismatch_count{};
  std::size_t compared_color_pixel_count{};
  std::size_t color_outlier_count{};
  double color_mean_absolute_error{};
  double color_outlier_ratio{};
  std::size_t compared_depth_pixel_count{};
  std::size_t depth_outlier_count{};
  double depth_outlier_ratio{};
};

enum class screenshot_comparison_error {
  none,
  invalid_dimensions,
  invalid_color_data,
  invalid_depth_data,
  invalid_options,
};

/**
 * 比较固定相机截图。轮廓边缘允许指定像素偏移，颜色和深度只统计双方一致的非边缘区域。
 * 输入和输出均由调用方拥有；失败时 output 保持不变。空 depth 表示不比较深度。
 */
[[nodiscard]] screenshot_comparison_error
compare_screenshots(screenshot_view expected, screenshot_view actual,
                    const screenshot_comparison_options& options,
                    screenshot_comparison_report& output) noexcept;

} // namespace granit::example::model_viewer

#endif
