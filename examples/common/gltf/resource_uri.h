// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_RESOURCE_URI_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_RESOURCE_URI_H_

#include <string>
#include <string_view>

namespace granit::example::gltf {

/** 规范化受控相对 URI；拒绝网络、绝对路径、转义和父目录跳转。 */
[[nodiscard]] bool normalize_resource_uri(std::string_view source, std::string& normalized);

} // namespace granit::example::gltf

#endif
