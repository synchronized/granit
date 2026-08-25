// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TIMESTAMP_QUERY_HPP_
#define GRANIT_TIMESTAMP_QUERY_HPP_

#include <span>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/timestamp_query.h>

namespace granit {

enum class timestamp_stage : std::uint32_t {
  top = GRANIT_TIMESTAMP_STAGE_TOP,
  draw = GRANIT_TIMESTAMP_STAGE_DRAW,
  bottom = GRANIT_TIMESTAMP_STAGE_BOTTOM,
};

class timestamp_query_pool {
public:
  timestamp_query_pool() = default;
  ~timestamp_query_pool() { static_cast<void>(reset()); }
  timestamp_query_pool(const timestamp_query_pool&) = delete;
  timestamp_query_pool& operator=(const timestamp_query_pool&) = delete;
  timestamp_query_pool(timestamp_query_pool&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  timestamp_query_pool& operator=(timestamp_query_pool&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer, std::uint32_t query_count) noexcept {
    if (valid())
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_timestamp_query_pool_desc desc{GRANIT_TIMESTAMP_QUERY_POOL_DESC_VERSION_1_SIZE,
                                                query_count, 0};
    const auto value = granit_timestamp_query_pool_create(renderer, &desc, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
  }
  [[nodiscard]] result get_results(std::uint32_t first,
                                   std::span<std::uint64_t> nanoseconds) noexcept {
    return from_native(granit_timestamp_query_pool_get_results(
        renderer_, handle_, first, static_cast<std::uint32_t>(nanoseconds.size()),
        nanoseconds.data()));
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_timestamp_query_pool_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_timestamp_query_pool native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_timestamp_query_pool handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
