// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_request.h"

#include <utility>

namespace granit::example::assets {

void asset_request::cancel() noexcept {
  auto expected = asset_request_status::pending;
  if (status_.compare_exchange_strong(expected, asset_request_status::cancelled,
                                      std::memory_order_acq_rel)) {
    generation_.fetch_add(1, std::memory_order_acq_rel);
    received_bytes_.store(0, std::memory_order_relaxed);
    total_bytes_.store(unknown_size, std::memory_order_relaxed);
    error_ = asset_request_error::cancelled;
    bytes_.clear();
    diagnostic_.clear();
  }
}

void asset_request::reset() noexcept {
  generation_.fetch_add(1, std::memory_order_acq_rel);
  status_.store(asset_request_status::idle, std::memory_order_release);
  received_bytes_.store(0, std::memory_order_relaxed);
  total_bytes_.store(unknown_size, std::memory_order_relaxed);
  error_ = asset_request_error::none;
  location_.clear();
  bytes_.clear();
  diagnostic_.clear();
}

asset_request_progress asset_request::progress() const noexcept {
  asset_request_progress result{.received_bytes = received_bytes_.load(std::memory_order_relaxed),
                                .total_bytes = std::nullopt};
  const auto total = total_bytes_.load(std::memory_order_relaxed);
  if (total != unknown_size)
    result.total_bytes = total;
  return result;
}

std::uint64_t asset_request_writer::begin(asset_request& request, std::string location) {
  const auto generation = request.generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
  request.location_ = std::move(location);
  request.bytes_.clear();
  request.diagnostic_.clear();
  request.error_ = asset_request_error::none;
  request.received_bytes_.store(0, std::memory_order_relaxed);
  request.total_bytes_.store(asset_request::unknown_size, std::memory_order_relaxed);
  request.status_.store(asset_request_status::pending, std::memory_order_release);
  return generation;
}

bool asset_request_writer::active(const asset_request& request, std::uint64_t generation) noexcept {
  return request.status_.load(std::memory_order_acquire) == asset_request_status::pending &&
         request.generation_.load(std::memory_order_acquire) == generation;
}

void asset_request_writer::progress(asset_request& request, std::uint64_t generation,
                                    std::uint64_t received_bytes,
                                    std::optional<std::uint64_t> total_bytes) noexcept {
  if (!active(request, generation))
    return;
  request.received_bytes_.store(received_bytes, std::memory_order_relaxed);
  request.total_bytes_.store(total_bytes.value_or(asset_request::unknown_size),
                             std::memory_order_relaxed);
}

bool asset_request_writer::complete(asset_request& request, std::uint64_t generation,
                                    std::span<const std::byte> bytes) {
  if (!active(request, generation) || bytes.empty())
    return false;
  request.bytes_.assign(bytes.begin(), bytes.end());
  request.diagnostic_.clear();
  request.error_ = asset_request_error::none;
  request.received_bytes_.store(bytes.size(), std::memory_order_relaxed);
  request.total_bytes_.store(bytes.size(), std::memory_order_relaxed);
  request.status_.store(asset_request_status::ready, std::memory_order_release);
  return true;
}

bool asset_request_writer::complete(asset_request& request, std::uint64_t generation,
                                    std::vector<std::byte>&& bytes) {
  if (!active(request, generation) || bytes.empty())
    return false;
  const auto size = bytes.size();
  request.bytes_ = std::move(bytes);
  request.diagnostic_.clear();
  request.error_ = asset_request_error::none;
  request.received_bytes_.store(size, std::memory_order_relaxed);
  request.total_bytes_.store(size, std::memory_order_relaxed);
  request.status_.store(asset_request_status::ready, std::memory_order_release);
  return true;
}

bool asset_request_writer::fail(asset_request& request, std::uint64_t generation,
                                asset_request_error error, std::string diagnostic) {
  if (!active(request, generation))
    return false;
  request.bytes_.clear();
  request.error_ = error;
  request.diagnostic_ = std::move(diagnostic);
  request.status_.store(asset_request_status::failed, std::memory_order_release);
  return true;
}

} // namespace granit::example::assets
