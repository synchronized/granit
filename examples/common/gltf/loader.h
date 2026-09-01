// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_LOADER_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_LOADER_H_

#include "gltf/scene.h"

#include <cstddef>
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
};

class resource_resolver {
public:
  resource_resolver() = default;
  virtual ~resource_resolver() = default;
  resource_resolver(const resource_resolver&) = delete;
  resource_resolver& operator=(const resource_resolver&) = delete;

  /** 返回资源自有字节；路径已经过规范化且不包含父目录跳转。 */
  [[nodiscard]] virtual bool resolve(std::string_view path,
                                     std::vector<std::byte>& bytes) const = 0;
};

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
                               const resource_resolver* resolver, scene& output);

} // namespace granit::example::gltf

#endif
