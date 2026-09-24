// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_MEMORY_RESOURCE_RESOLVER_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_MEMORY_RESOURCE_RESOLVER_H_

#include "assets/resource_resolver.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace granit::example::assets {

/** 保存预取的 glTF 外部资源，并按规范化 URI 提供只读解析。 */
class memory_resource_resolver final : public resource_resolver {
public:
  [[nodiscard]] bool insert(std::string_view resource_uri, std::span<const std::byte> bytes);
  [[nodiscard]] bool contains(std::string_view resource_uri) const;
  [[nodiscard]] bool resolve(std::string_view resource_uri,
                             std::vector<std::byte>& bytes) const override;
  void clear() noexcept;
  void swap(memory_resource_resolver& other) noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return resources_.size(); }

private:
  std::unordered_map<std::string, std::vector<std::byte>> resources_;
};

} // namespace granit::example::assets

#endif
