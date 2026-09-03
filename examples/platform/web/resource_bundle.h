// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_PLATFORM_WEB_RESOURCE_BUNDLE_H_
#define GRANIT_EXAMPLES_PLATFORM_WEB_RESOURCE_BUNDLE_H_

#include "gltf/loader.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace granit::example::model_viewer::web {

/** 保存浏览器预取的 glTF 外部资源，并按规范化 URI 提供只读解析。 */
class resource_bundle final : public gltf::resource_resolver {
public:
  [[nodiscard]] bool insert(std::string_view path, std::span<const std::byte> bytes);
  [[nodiscard]] bool contains(std::string_view path) const;
  [[nodiscard]] bool resolve(std::string_view path, std::vector<std::byte>& bytes) const override;
  void clear() noexcept;
  void swap(resource_bundle& other) noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return resources_.size(); }

private:
  std::unordered_map<std::string, std::vector<std::byte>> resources_;
};

} // namespace granit::example::model_viewer::web

#endif
