// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SYSTEM_RESOLVER_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SYSTEM_RESOLVER_H_

#include "assets/asset_system.h"
#include "assets/resource_resolver.h"

#include <string>

namespace granit::example::assets {

/** 从已驻留的 Asset System 挂载同步解析相对资源。 */
class asset_system_resolver final : public resource_resolver {
public:
  asset_system_resolver(asset_system& assets, asset_mount mount, std::string base_path);

  [[nodiscard]] bool resolve(std::string_view path, std::vector<std::byte>& output) const override;

private:
  asset_system& assets_;
  asset_mount mount_;
  std::string base_path_;
};

} // namespace granit::example::assets

#endif // GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SYSTEM_RESOLVER_H_
