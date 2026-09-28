// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_PLATFORM_REGISTER_ASSET_SOURCES_H_
#define GRANIT_EXAMPLES_COMMON_PLATFORM_REGISTER_ASSET_SOURCES_H_

#include <granit/core/result.hpp>

#include <string_view>

namespace granit::example::assets {
class asset_manager;
}

namespace granit::example::platform {

/** 为当前平台注册 bundled、file 和可用的网络 Asset Source。 */
[[nodiscard]] granit::result register_asset_sources(assets::asset_manager& manager,
                                                    std::string_view executable_path) noexcept;

} // namespace granit::example::platform

#endif
