// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_RESOURCE_PATH_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_RESOURCE_PATH_H_

#include <string>
#include <string_view>

namespace granit::example::assets {

/** 规范化受控相对资源路径；拒绝网络位置、绝对路径、转义和父目录跳转。 */
[[nodiscard]] bool normalize_resource_path(std::string_view source, std::string& normalized);

} // namespace granit::example::assets

#endif
