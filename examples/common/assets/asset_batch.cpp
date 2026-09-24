// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_batch.h"

#include "assets/resource_path.h"

#include <algorithm>
#include <utility>

namespace granit::example::assets {

bool asset_batch::add(std::string_view path, std::string location) {
  std::string normalized_path;
  if (started_ || location.empty() || location.find('\0') != std::string::npos ||
      !normalize_resource_path(path, normalized_path)) {
    return false;
  }
  if (std::ranges::any_of(entries_, [&](const asset_batch_entry& entry) {
        return entry.path == normalized_path;
      })) {
    return false;
  }
  entries_.push_back(
      {.path = std::move(normalized_path), .location = std::move(location), .request = {}});
  return true;
}

bool asset_batch::start(asset_loader& loader) {
  if (started_)
    return false;
  started_ = true;
  for (auto& entry : entries_)
    entry.request = loader.load(entry.location);
  return true;
}

asset_batch_status asset_batch::status() const noexcept {
  if (!started_)
    return asset_batch_status::idle;
  bool pending = false;
  for (const auto& entry : entries_) {
    if (!entry.request)
      return asset_batch_status::failed;
    switch (entry.request->status()) {
    case asset_request_status::failed:
      return asset_batch_status::failed;
    case asset_request_status::cancelled:
      return asset_batch_status::cancelled;
    case asset_request_status::pending:
    case asset_request_status::idle:
      pending = true;
      break;
    case asset_request_status::ready:
      break;
    }
  }
  return pending ? asset_batch_status::pending : asset_batch_status::ready;
}

asset_batch_progress asset_batch::progress() const noexcept {
  asset_batch_progress result{.total_items = entries_.size(), .total_bytes = 0};
  for (const auto& entry : entries_) {
    if (!entry.request) {
      result.total_bytes.reset();
      continue;
    }
    if (entry.request->status() == asset_request_status::ready)
      ++result.completed_items;
    const auto request_progress = entry.request->progress();
    result.received_bytes += request_progress.received_bytes;
    if (result.total_bytes && request_progress.total_bytes)
      *result.total_bytes += *request_progress.total_bytes;
    else
      result.total_bytes.reset();
  }
  return result;
}

bool asset_batch::commit(memory_resource_resolver& resolver) const {
  if (status() != asset_batch_status::ready)
    return false;
  memory_resource_resolver replacement;
  for (const auto& entry : entries_) {
    if (!replacement.insert(entry.path, entry.request->bytes()))
      return false;
  }
  resolver.swap(replacement);
  return true;
}

void asset_batch::cancel() noexcept {
  for (const auto& entry : entries_) {
    if (entry.request)
      entry.request->cancel();
  }
}

void asset_batch::clear() noexcept {
  cancel();
  entries_.clear();
  started_ = false;
}

} // namespace granit::example::assets
