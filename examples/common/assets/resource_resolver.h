// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_RESOURCE_RESOLVER_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_RESOURCE_RESOLVER_H_

#include <cstddef>
#include <string_view>
#include <vector>

namespace granit::example::assets {

/** 按文档中的受控相对资源 URI 返回自有字节。 */
class resource_resolver {
public:
  resource_resolver() = default;
  virtual ~resource_resolver() = default;
  resource_resolver(const resource_resolver&) = delete;
  resource_resolver& operator=(const resource_resolver&) = delete;

  [[nodiscard]] virtual bool resolve(std::string_view resource_uri,
                                     std::vector<std::byte>& bytes) const = 0;
};

} // namespace granit::example::assets

#endif
