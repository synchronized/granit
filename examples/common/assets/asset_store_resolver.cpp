// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_store_resolver.h"

#include <utility>

namespace granit::example::assets {

asset_store_resolver::asset_store_resolver(const asset_store& assets, std::string base_path)
    : assets_(assets), base_path_(std::move(base_path)) {
  if (!base_path_.empty() && base_path_.back() != '/')
    base_path_.push_back('/');
}

bool asset_store_resolver::resolve(std::string_view path, std::vector<std::byte>& output) const {
  return assets_.read(base_path_ + std::string{path}, output);
}

} // namespace granit::example::assets
