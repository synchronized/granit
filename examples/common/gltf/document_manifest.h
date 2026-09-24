// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_DOCUMENT_MANIFEST_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_DOCUMENT_MANIFEST_H_

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace granit::example::gltf {

enum class document_manifest_error {
  none,
  invalid_document,
  truncated_data,
  invalid_resource_uri,
  out_of_memory,
};

struct document_manifest_result {
  document_manifest_error error{document_manifest_error::none};
  std::string diagnostic;

  [[nodiscard]] explicit operator bool() const noexcept {
    return error == document_manifest_error::none;
  }
};

/** 扫描 GLB 或 glTF 文档引用的安全外部资源 URI；失败时 output 保持不变。 */
[[nodiscard]] document_manifest_result
discover_external_resources(std::span<const std::byte> document, std::vector<std::string>& output);

} // namespace granit::example::gltf

#endif
