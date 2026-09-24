// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_store.h"

#include "assets/resource_path.h"

#include <cstdint>
#include <fstream>
#include <limits>
#include <string>

namespace granit::example::assets {
bool asset_store::initialize(std::string_view executable_path) {
#if defined(__EMSCRIPTEN__)
  static_cast<void>(executable_path);
  root_ = "/assets";
#else
  if (executable_path.empty())
    return false;
  std::error_code error;
  auto executable = std::filesystem::absolute(std::filesystem::path{executable_path}, error);
  if (error)
    return false;
  root_ = executable.parent_path() / "assets";
#endif
  return true;
}

bool asset_store::read(std::string_view logical_path, std::vector<std::byte>& output) const {
  std::string normalized;
  if (root_.empty() || !normalize_resource_path(logical_path, normalized))
    return false;

  const auto path = root_ / std::filesystem::path{normalized};
  std::ifstream stream{path, std::ios::binary | std::ios::ate};
  if (!stream)
    return false;
  const auto end = stream.tellg();
  if (end <= 0)
    return false;
  const auto size = static_cast<std::uint64_t>(end);
  if (size > std::numeric_limits<std::size_t>::max() ||
      size > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
    return false;
  }

  std::vector<std::byte> candidate(static_cast<std::size_t>(size));
  stream.seekg(0, std::ios::beg);
  stream.read(reinterpret_cast<char*>(candidate.data()), static_cast<std::streamsize>(size));
  if (!stream)
    return false;
  output = std::move(candidate);
  return true;
}

} // namespace granit::example::assets
