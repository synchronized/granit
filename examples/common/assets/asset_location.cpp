// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_location.h"

#include "assets/resource_path.h"

#include <filesystem>
#include <string>
#include <utility>

namespace granit::example::assets {

bool resolve_asset_location(std::string_view base, std::string_view relative, std::string& output) {
  if (base.empty() || relative.empty() || base.find('\0') != base.npos ||
      relative.find('\0') != relative.npos) {
    return false;
  }

  std::string candidate;
  if (relative.starts_with("http://") || relative.starts_with("https://") ||
      relative.starts_with('/')) {
    candidate = relative;
  } else {
    std::string normalized;
    if (!normalize_resource_path(relative, normalized))
      return false;
    if (base.starts_with("http://") || base.starts_with("https://")) {
      const auto suffix = base.find_first_of("?#");
      const auto path = base.substr(0, suffix);
      const auto separator = path.find_last_of('/');
      candidate = separator == path.npos ? normalized
                                         : std::string{path.substr(0, separator + 1)} + normalized;
    } else {
      const auto base_path = std::filesystem::path{std::string{base}};
      candidate =
          (base_path.parent_path() / std::filesystem::path{normalized}).lexically_normal().string();
    }
  }
  if (candidate.empty())
    return false;
  output = std::move(candidate);
  return true;
}

} // namespace granit::example::assets
