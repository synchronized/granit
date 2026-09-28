// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_location.h"

#include "assets/resource_path.h"

namespace granit::example::assets {
asset_location asset_location::bundled(std::string path) {
  std::string normalized_path;
  const bool valid = normalize_resource_path(path, normalized_path);
  return {asset_scheme::bundled, std::move(normalized_path), valid};
}

asset_location asset_location::external(std::string location) {
  if (location.empty() || location.find('\0') != std::string::npos)
    return {};
  if (location.starts_with("https://"))
    return {asset_scheme::https, std::move(location), true};
  if (location.starts_with("http://"))
    return {asset_scheme::http, std::move(location), true};
  return {asset_scheme::file, std::move(location), true};
}

asset_location asset_location::memory(std::string path) {
  std::string normalized_path;
  const bool valid = normalize_resource_path(path, normalized_path);
  return {asset_scheme::memory, std::move(normalized_path), valid};
}

std::string asset_location::key() const {
  return std::to_string(static_cast<std::uint32_t>(scheme_)) + ':' + path_;
}

} // namespace granit::example::assets
