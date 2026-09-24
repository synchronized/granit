// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_LOADER_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_LOADER_H_

#include "assets/resource_resolver.h"
#include "gltf/scene.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace granit::example::gltf {

enum class load_error {
  none,
  invalid_document,
  truncated_data,
  invalid_resource_uri,
  missing_resource,
  accessor_out_of_bounds,
  unsupported_feature,
  image_decode_failed,
  numeric_overflow,
  out_of_memory,
  cancelled,
};

enum class load_stage { document, buffers, images, materials, meshes, nodes };

struct load_progress {
  load_stage stage{load_stage::document};
  std::uint32_t completed{};
  std::uint32_t total{};
};

/** 返回 false 可在阶段边界取消加载。 */
using load_progress_callback = bool (*)(const load_progress& progress, void* user_data);

struct load_result {
  load_error error{load_error::none};
  std::string diagnostic;

  [[nodiscard]] explicit operator bool() const noexcept { return error == load_error::none; }
};

/** 解析文档引用的安全外部资源 URI；失败时 output 保持不变。 */
[[nodiscard]] load_result discover_external_resources(std::span<const std::byte> document,
                                                      std::vector<std::string>& output);

/** 解析 GLB 或 glTF；失败时 output 保持不变。 */
[[nodiscard]] load_result load(std::span<const std::byte> document,
                               const assets::resource_resolver* resolver, scene& output,
                               load_progress_callback progress = nullptr,
                               void* progress_user_data = nullptr);

} // namespace granit::example::gltf

#endif
