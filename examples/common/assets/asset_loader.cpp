// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_loader.h"

#include <new>

namespace granit::example::assets {

std::type_index blob_asset_loader::target_type() const noexcept { return typeid(asset_blob); }

bool blob_asset_loader::accepts(const asset_location&, std::span<const std::byte>) const noexcept {
  return true;
}

asset_decode_result blob_asset_loader::decode(const asset_location&,
                                              std::span<const std::byte> bytes) noexcept {
  try {
    auto blob = std::make_shared<asset_blob>();
    blob->bytes.assign(bytes.begin(), bytes.end());
    return {.error = asset_error::none, .value = std::move(blob), .diagnostic = {}};
  } catch (const std::bad_alloc&) {
    return {.error = asset_error::out_of_memory,
            .value = {},
            .diagnostic = "无法分配 Blob 资产"};
  } catch (...) {
    return {.error = asset_error::internal,
            .value = {},
            .diagnostic = "Blob Loader 发生内部错误"};
  }
}

} // namespace granit::example::assets
