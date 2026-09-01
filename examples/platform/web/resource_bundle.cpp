// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "resource_bundle.h"

#include "gltf/resource_uri.h"

#include <string>

namespace granit::example::model_viewer::web {

bool resource_bundle::insert(std::string_view path, std::span<const std::byte> bytes) {
  std::string normalized;
  if (bytes.empty() || !gltf::normalize_resource_uri(path, normalized))
    return false;
  std::vector<std::byte> candidate(bytes.begin(), bytes.end());
  resources_.insert_or_assign(std::move(normalized), std::move(candidate));
  return true;
}

bool resource_bundle::contains(std::string_view path) const {
  std::string normalized;
  return gltf::normalize_resource_uri(path, normalized) && resources_.contains(normalized);
}

bool resource_bundle::resolve(std::string_view path, std::vector<std::byte>& bytes) const {
  std::string normalized;
  if (!gltf::normalize_resource_uri(path, normalized))
    return false;
  const auto found = resources_.find(normalized);
  if (found == resources_.end())
    return false;
  bytes = found->second;
  return true;
}

void resource_bundle::clear() noexcept { resources_.clear(); }

} // namespace granit::example::model_viewer::web
