// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "resource_fetch_batch.h"

#include "gltf/resource_uri.h"

#include <algorithm>
#include <utility>

namespace granit::example::model_viewer::web {

bool resource_fetch_batch::add(std::string_view path, std::string url) {
  std::string normalized_path;
  if (url.empty() || url.find('\0') != std::string::npos ||
      !gltf::normalize_resource_uri(path, normalized_path)) {
    return false;
  }
  if (std::ranges::any_of(entries_, [&](const resource_fetch_entry& entry) {
        return entry.path == normalized_path;
      })) {
    return false;
  }

  entries_.push_back(
      resource_fetch_entry{std::move(normalized_path), std::move(url),
                           std::make_shared<asset_request>()});
  return true;
}

resource_fetch_batch_status resource_fetch_batch::status() const noexcept {
  if (entries_.empty())
    return resource_fetch_batch_status::idle;

  bool has_pending = false;
  bool has_idle = false;
  for (const auto& entry : entries_) {
    switch (entry.request->status()) {
    case asset_request_status::failed:
      return resource_fetch_batch_status::failed;
    case asset_request_status::pending:
      has_pending = true;
      break;
    case asset_request_status::idle:
      has_idle = true;
      break;
    case asset_request_status::ready:
      break;
    }
  }
  if (has_pending)
    return resource_fetch_batch_status::pending;
  if (has_idle)
    return resource_fetch_batch_status::idle;
  return resource_fetch_batch_status::ready;
}

bool resource_fetch_batch::commit(resource_bundle& bundle) const {
  if (status() != resource_fetch_batch_status::ready)
    return false;

  resource_bundle replacement;
  for (const auto& entry : entries_) {
    if (!replacement.insert(entry.path, entry.request->bytes()))
      return false;
  }
  bundle.swap(replacement);
  return true;
}

void resource_fetch_batch::clear() noexcept { entries_.clear(); }

} // namespace granit::example::model_viewer::web
