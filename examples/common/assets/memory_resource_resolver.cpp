// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/memory_resource_resolver.h"

#include "assets/resource_path.h"

#include <string>

namespace granit::example::assets {

bool memory_resource_resolver::insert(std::string_view path, std::span<const std::byte> bytes) {
  std::string normalized;
  if (bytes.empty() || !normalize_resource_path(path, normalized))
    return false;
  std::vector<std::byte> candidate(bytes.begin(), bytes.end());
  resources_.insert_or_assign(std::move(normalized), std::move(candidate));
  return true;
}

bool memory_resource_resolver::contains(std::string_view path) const {
  std::string normalized;
  return normalize_resource_path(path, normalized) && resources_.contains(normalized);
}

bool memory_resource_resolver::resolve(std::string_view path, std::vector<std::byte>& bytes) const {
  std::string normalized;
  if (!normalize_resource_path(path, normalized))
    return false;
  const auto found = resources_.find(normalized);
  if (found == resources_.end())
    return false;
  bytes = found->second;
  return true;
}

void memory_resource_resolver::clear() noexcept { resources_.clear(); }

void memory_resource_resolver::swap(memory_resource_resolver& other) noexcept {
  resources_.swap(other.resources_);
}

} // namespace granit::example::assets
