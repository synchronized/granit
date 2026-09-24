// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_system_resolver.h"

#include <utility>

namespace granit::example::assets {

asset_system_resolver::asset_system_resolver(asset_system& assets, asset_mount mount,
                                             std::string base_path)
    : assets_(assets), mount_(mount), base_path_(std::move(base_path)) {
  if (!base_path_.empty() && base_path_.back() != '/')
    base_path_.push_back('/');
}

bool asset_system_resolver::resolve(std::string_view path, std::vector<std::byte>& output) const {
  const auto request = assets_.request({mount_, base_path_ + std::string{path}});
  if (!request || request->status() != asset_request_status::ready)
    return false;
  output = request->bytes();
  return true;
}

} // namespace granit::example::assets
