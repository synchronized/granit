// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/screenshot_comparison.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace granit::example::model_viewer {
namespace {

bool valid_size(screenshot_view view, std::size_t& pixel_count) noexcept {
  if (view.width == 0 || view.height == 0 ||
      view.width > std::numeric_limits<std::size_t>::max() / view.height) {
    return false;
  }
  pixel_count = static_cast<std::size_t>(view.width) * view.height;
  return pixel_count <= std::numeric_limits<std::size_t>::max() / 4;
}

bool foreground(std::span<const std::uint8_t> rgba, std::size_t pixel,
                const screenshot_comparison_options& options) noexcept {
  const auto offset = pixel * 4;
  for (std::size_t channel = 0; channel < 4; ++channel) {
    const auto difference = std::abs(static_cast<int>(rgba[offset + channel]) -
                                     static_cast<int>(options.background[channel]));
    if (difference > options.background_tolerance)
      return true;
  }
  return false;
}

bool near_category(std::span<const std::uint8_t> rgba, std::uint32_t width, std::uint32_t height,
                   std::uint32_t x, std::uint32_t y, bool category, std::uint8_t radius,
                   const screenshot_comparison_options& options) noexcept {
  const auto x_begin = x > radius ? x - radius : 0U;
  const auto y_begin = y > radius ? y - radius : 0U;
  const auto x_end = x + std::min<std::uint32_t>(radius, width - 1 - x);
  const auto y_end = y + std::min<std::uint32_t>(radius, height - 1 - y);
  for (auto sample_y = y_begin; sample_y <= y_end; ++sample_y) {
    for (auto sample_x = x_begin; sample_x <= x_end; ++sample_x) {
      if (foreground(rgba, static_cast<std::size_t>(sample_y) * width + sample_x, options) ==
          category) {
        return true;
      }
    }
  }
  return false;
}

bool is_edge(std::span<const std::uint8_t> rgba, std::uint32_t width, std::uint32_t height,
             std::uint32_t x, std::uint32_t y,
             const screenshot_comparison_options& options) noexcept {
  const auto category = foreground(rgba, static_cast<std::size_t>(y) * width + x, options);
  return near_category(rgba, width, height, x, y, !category, options.edge_tolerance_pixels,
                       options);
}

} // namespace

screenshot_comparison_error compare_screenshots(screenshot_view expected, screenshot_view actual,
                                                const screenshot_comparison_options& options,
                                                screenshot_comparison_report& output) noexcept {
  std::size_t expected_pixels{};
  std::size_t actual_pixels{};
  if (!valid_size(expected, expected_pixels) || !valid_size(actual, actual_pixels) ||
      expected.width != actual.width || expected.height != actual.height) {
    return screenshot_comparison_error::invalid_dimensions;
  }
  if (expected.rgba.size() != expected_pixels * 4 || actual.rgba.size() != actual_pixels * 4)
    return screenshot_comparison_error::invalid_color_data;
  if (expected.depth.empty() != actual.depth.empty() ||
      (!expected.depth.empty() &&
       (expected.depth.size() != expected_pixels || actual.depth.size() != actual_pixels))) {
    return screenshot_comparison_error::invalid_depth_data;
  }
  if (options.max_color_mean_absolute_error < 0.0 || options.max_color_outlier_ratio < 0.0 ||
      options.max_color_outlier_ratio > 1.0 || options.depth_absolute_tolerance < 0.0F ||
      options.max_depth_outlier_ratio < 0.0 || options.max_depth_outlier_ratio > 1.0) {
    return screenshot_comparison_error::invalid_options;
  }

  screenshot_comparison_report candidate;
  double color_error_sum{};
  for (std::uint32_t y = 0; y < expected.height; ++y) {
    for (std::uint32_t x = 0; x < expected.width; ++x) {
      const auto pixel = static_cast<std::size_t>(y) * expected.width + x;
      const auto expected_foreground = foreground(expected.rgba, pixel, options);
      const auto actual_foreground = foreground(actual.rgba, pixel, options);
      if (expected_foreground != actual_foreground) {
        const auto tolerated =
            near_category(expected.rgba, expected.width, expected.height, x, y, actual_foreground,
                          options.edge_tolerance_pixels, options) &&
            near_category(actual.rgba, actual.width, actual.height, x, y, expected_foreground,
                          options.edge_tolerance_pixels, options);
        if (!tolerated)
          ++candidate.silhouette_mismatch_count;
        continue;
      }
      if (is_edge(expected.rgba, expected.width, expected.height, x, y, options) ||
          is_edge(actual.rgba, actual.width, actual.height, x, y, options)) {
        continue;
      }

      ++candidate.compared_color_pixel_count;
      bool color_outlier = false;
      const auto offset = pixel * 4;
      for (std::size_t channel = 0; channel < 4; ++channel) {
        const auto difference = std::abs(static_cast<int>(expected.rgba[offset + channel]) -
                                         static_cast<int>(actual.rgba[offset + channel]));
        color_error_sum += difference;
        color_outlier = color_outlier || difference > options.color_channel_threshold;
      }
      if (color_outlier)
        ++candidate.color_outlier_count;

      if (!expected.depth.empty() && expected_foreground) {
        ++candidate.compared_depth_pixel_count;
        const auto expected_depth = expected.depth[pixel];
        const auto actual_depth = actual.depth[pixel];
        if (!std::isfinite(expected_depth) || !std::isfinite(actual_depth) ||
            expected_depth < 0.0F || expected_depth > 1.0F || actual_depth < 0.0F ||
            actual_depth > 1.0F ||
            std::abs(expected_depth - actual_depth) > options.depth_absolute_tolerance) {
          ++candidate.depth_outlier_count;
        }
      }
    }
  }

  if (candidate.compared_color_pixel_count != 0) {
    candidate.color_mean_absolute_error =
        color_error_sum / (static_cast<double>(candidate.compared_color_pixel_count) * 4.0);
    candidate.color_outlier_ratio =
        static_cast<double>(candidate.color_outlier_count) / candidate.compared_color_pixel_count;
  }
  if (candidate.compared_depth_pixel_count != 0) {
    candidate.depth_outlier_ratio =
        static_cast<double>(candidate.depth_outlier_count) / candidate.compared_depth_pixel_count;
  }
  candidate.passed = candidate.silhouette_mismatch_count == 0 &&
                     candidate.compared_color_pixel_count != 0 &&
                     (expected.depth.empty() || candidate.compared_depth_pixel_count != 0) &&
                     candidate.color_mean_absolute_error <= options.max_color_mean_absolute_error &&
                     candidate.color_outlier_ratio <= options.max_color_outlier_ratio &&
                     candidate.depth_outlier_ratio <= options.max_depth_outlier_ratio;
  output = candidate;
  return screenshot_comparison_error::none;
}

} // namespace granit::example::model_viewer
