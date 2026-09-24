// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_IMPORTER_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_IMPORTER_H_

#include "assets/resource_resolver.h"
#include "gltf/scene.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace granit::example::gltf {

enum class import_error {
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

enum class import_stage { document, buffers, images, materials, meshes, nodes };

struct import_progress {
  import_stage stage{import_stage::document};
  std::uint32_t completed{};
  std::uint32_t total{};
};

/** 返回 false 可在阶段边界取消加载。 */
using import_progress_callback = bool (*)(const import_progress& progress, void* user_data);

struct import_result {
  import_error error{import_error::none};
  std::string diagnostic;

  [[nodiscard]] explicit operator bool() const noexcept { return error == import_error::none; }
};

/** 将 GLB 或 glTF 文档导入 CPU Scene；失败时 output 保持不变。 */
[[nodiscard]] import_result import_scene(std::span<const std::byte> document,
                                         const assets::resource_resolver* resolver, scene& output,
                                         import_progress_callback progress = nullptr,
                                         void* progress_user_data = nullptr);

} // namespace granit::example::gltf

#endif
