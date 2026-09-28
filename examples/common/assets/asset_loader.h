// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_LOADER_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_LOADER_H_

#include "assets/asset_handle.h"
#include "assets/asset_location.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <typeindex>
#include <vector>

namespace granit::example::assets {

struct asset_blob {
  std::vector<std::byte> bytes;
};

struct asset_decode_result {
  asset_error error{asset_error::none};
  std::shared_ptr<const void> value;
  std::string diagnostic;

  [[nodiscard]] bool succeeded() const noexcept {
    return error == asset_error::none && value != nullptr;
  }
};

/** 只负责把已经读取的字节解码为一个目标 CPU 资产。 */
class asset_loader {
public:
  virtual ~asset_loader() = default;
  [[nodiscard]] virtual std::type_index target_type() const noexcept = 0;
  [[nodiscard]] virtual bool accepts(const asset_location& location,
                                     std::span<const std::byte> bytes) const noexcept = 0;
  [[nodiscard]] virtual asset_decode_result decode(const asset_location& location,
                                                   std::span<const std::byte> bytes) noexcept = 0;
};

/** Blob Loader 不解释内容，只把读取结果移入稳定 CPU 资产。 */
class blob_asset_loader final : public asset_loader {
public:
  [[nodiscard]] std::type_index target_type() const noexcept override;
  [[nodiscard]] bool accepts(const asset_location&, std::span<const std::byte>) const noexcept override;
  [[nodiscard]] asset_decode_result decode(const asset_location&,
                                           std::span<const std::byte> bytes) noexcept override;
};

} // namespace granit::example::assets

#endif
