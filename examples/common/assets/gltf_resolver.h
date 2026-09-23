// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_GLTF_RESOLVER_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_GLTF_RESOLVER_H_

#include "assets/asset_store.h"
#include "gltf/loader.h"

#include <string>

namespace granit::example::assets {

/** 将 glTF 相对 URI 映射到 Asset Store 的一个逻辑目录。 */
class gltf_resolver final : public gltf::resource_resolver {
public:
  gltf_resolver(const asset_store& assets, std::string base_path);

  [[nodiscard]] bool resolve(std::string_view path, std::vector<std::byte>& output) const override;

private:
  const asset_store& assets_;
  std::string base_path_;
};

} // namespace granit::example::assets

#endif
