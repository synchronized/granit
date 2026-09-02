// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_ENVIRONMENT_PACKAGE_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_ENVIRONMENT_PACKAGE_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace granit::example::model_viewer {

enum class environment_package_error {
  none,
  truncated,
  invalid_magic,
  unsupported_version,
  invalid_layout,
  size_overflow,
};

struct environment_mip {
  std::uint32_t resolution{};
  std::span<const std::byte> pixels;
};

/** 借用 GRENV 文件字节；调用方必须保证源数据在使用期间保持有效。 */
struct environment_package {
  std::uint32_t irradiance_resolution{};
  std::span<const std::byte> irradiance_pixels;
  std::vector<environment_mip> prefiltered_mips;
  std::uint32_t brdf_width{};
  std::uint32_t brdf_height{};
  std::span<const std::byte> brdf_pixels;
};

/** 解析 example 私有 GRENV v1；像素固定为紧密排列的 RGBA16F。 */
[[nodiscard]] environment_package_error parse_environment_package(std::span<const std::byte> bytes,
                                                                  environment_package& package);

} // namespace granit::example::model_viewer

#endif
