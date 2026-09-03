// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_ENVIRONMENT_KTX2_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_ENVIRONMENT_KTX2_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace granit::example::model_viewer {

enum class environment_ktx2_error {
  none,
  truncated,
  invalid_identifier,
  unsupported_layout,
  invalid_level,
};

struct environment_ktx2_level {
  std::uint32_t resolution{};
  std::span<const std::byte> pixels;
};

/** 只借用未压缩 RGBA16F、六面的二维 KTX2 Cube 数据。 */
struct environment_ktx2_cube {
  std::vector<environment_ktx2_level> levels;
};

[[nodiscard]] environment_ktx2_error parse_environment_ktx2_cube(std::span<const std::byte> bytes,
                                                                 environment_ktx2_cube& cube);

} // namespace granit::example::model_viewer

#endif
