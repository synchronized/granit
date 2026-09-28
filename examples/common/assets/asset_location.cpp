// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_location.h"

#include "assets/resource_path.h"

#include <filesystem>

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

asset_location asset_location::resolve(std::string_view relative_path) const {
  if (!valid_)
    return {};
  std::string normalized_relative;
  if (!normalize_resource_path(relative_path, normalized_relative))
    return {};
  if (scheme_ == asset_scheme::file) {
    const auto resolved =
        (std::filesystem::path{path_}.parent_path() / normalized_relative).lexically_normal();
    return {scheme_, resolved.string(), !resolved.empty()};
  }
  if (scheme_ == asset_scheme::http || scheme_ == asset_scheme::https) {
    if (path_.find_first_of("?#") != std::string::npos)
      return {};
    const auto separator = path_.find_last_of('/');
    if (separator == std::string::npos)
      return {};
    return {scheme_, path_.substr(0, separator + 1) + normalized_relative, true};
  }
  std::string resolved;
  if (!resolve_resource_path(path_, normalized_relative, resolved))
    return {};
  return {scheme_, std::move(resolved), true};
}

} // namespace granit::example::assets
