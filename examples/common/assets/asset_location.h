// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_LOCATION_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_LOCATION_H_

#include <string>
#include <string_view>

namespace granit::example::assets {

/** 相对主资产位置解析受控资源路径；失败时 output 保持不变。 */
[[nodiscard]] bool resolve_asset_location(std::string_view base, std::string_view relative,
                                          std::string& output);

} // namespace granit::example::assets

#endif // GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_LOCATION_H_
