// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_ASSET_LOADERS_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_ASSET_LOADERS_H_

#include <granit/core/result.hpp>

namespace granit::example::assets {
class asset_manager;
}

namespace granit::example::gltf {

/** 注册图片与 glTF Scene Loader；应在第一次资产加载之前调用。 */
[[nodiscard]] granit::result
register_standard_asset_loaders(assets::asset_manager& manager) noexcept;

} // namespace granit::example::gltf

#endif
