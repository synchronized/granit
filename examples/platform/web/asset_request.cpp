// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_request.h"

#include <limits>
#include <utility>

namespace granit::example::model_viewer::web {

std::uint64_t asset_request::begin(std::string url) {
  if (generation_ == std::numeric_limits<std::uint64_t>::max())
    generation_ = 1;
  else
    ++generation_;
  status_ = asset_request_status::pending;
  url_ = std::move(url);
  bytes_.clear();
  diagnostic_.clear();
  return generation_;
}

bool asset_request::complete(std::uint64_t generation, std::span<const std::byte> bytes) {
  if (status_ != asset_request_status::pending || generation != generation_ || bytes.empty())
    return false;
  bytes_.assign(bytes.begin(), bytes.end());
  diagnostic_.clear();
  status_ = asset_request_status::ready;
  return true;
}

bool asset_request::fail(std::uint64_t generation, std::string diagnostic) {
  if (status_ != asset_request_status::pending || generation != generation_)
    return false;
  bytes_.clear();
  diagnostic_ = std::move(diagnostic);
  status_ = asset_request_status::failed;
  return true;
}

void asset_request::cancel() noexcept {
  if (status_ == asset_request_status::pending) {
    status_ = asset_request_status::idle;
    url_.clear();
    bytes_.clear();
    diagnostic_.clear();
  }
}

void asset_request::reset() noexcept {
  status_ = asset_request_status::idle;
  url_.clear();
  bytes_.clear();
  diagnostic_.clear();
}

} // namespace granit::example::model_viewer::web
