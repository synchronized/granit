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
  float recommended_environment_intensity{0.4F};
  float recommended_exposure_ev{-0.5F};
  std::uint32_t irradiance_resolution{};
  std::span<const std::byte> irradiance_pixels;
  std::vector<environment_mip> prefiltered_mips;
  std::uint32_t brdf_width{};
  std::uint32_t brdf_height{};
  std::span<const std::byte> brdf_pixels;
};

/** 解析 example 私有 GRENV v2；像素固定为紧密排列的 RGBA16F。 */
[[nodiscard]] environment_package_error parse_environment_package(std::span<const std::byte> bytes,
                                                                  environment_package& package);

/** 将已验证的 RGBA16F 像素编码为确定性的 GRENV v2；失败时 output 保持不变。 */
[[nodiscard]] environment_package_error
encode_environment_package(const environment_package& package, std::vector<std::byte>& output);

} // namespace granit::example::model_viewer

#endif
