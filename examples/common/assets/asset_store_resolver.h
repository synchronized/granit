// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_STORE_RESOLVER_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_STORE_RESOLVER_H_

#include "assets/asset_store.h"
#include "assets/resource_resolver.h"

#include <string>

namespace granit::example::assets {

/** 将相对资源路径映射到 Asset Store 的一个逻辑目录。 */
class asset_store_resolver final : public resource_resolver {
public:
  asset_store_resolver(const asset_store& assets, std::string base_path);

  [[nodiscard]] bool resolve(std::string_view path, std::vector<std::byte>& output) const override;

private:
  const asset_store& assets_;
  std::string base_path_;
};

} // namespace granit::example::assets

#endif
