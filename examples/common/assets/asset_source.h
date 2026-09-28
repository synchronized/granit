// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SOURCE_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SOURCE_H_

#include "assets/asset_handle.h"
#include "assets/asset_location.h"

#include <granit/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace granit::example::assets {

struct asset_source_result {
  asset_error error{asset_error::none};
  std::vector<std::byte> bytes;
  std::optional<std::uint64_t> total_bytes;
  std::string diagnostic;

  [[nodiscard]] bool succeeded() const noexcept { return error == asset_error::none; }
};

using asset_source_completion = std::function<void(asset_source_result)>;

/** 只负责把 Asset Location 异步读取为自有字节。 */
class asset_source {
public:
  virtual ~asset_source() = default;
  [[nodiscard]] virtual granit::result load(const asset_location& location,
                                            asset_source_completion completion) noexcept = 0;
};

} // namespace granit::example::assets

#endif
