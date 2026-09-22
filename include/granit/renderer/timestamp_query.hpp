// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TIMESTAMP_QUERY_HPP_
#define GRANIT_TIMESTAMP_QUERY_HPP_

#include <span>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/async_operation.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/timestamp_query.h>

namespace granit {

class timestamp_query_pool;

/** 不拥有 Timestamp Query Pool，只在来源 Pool 的有效期内使用。 */
class timestamp_query_pool_ref {
public:
  timestamp_query_pool_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_timestamp_query_pool native_handle() const noexcept {
    return handle_;
  }
  [[nodiscard]] static constexpr timestamp_query_pool_ref
  from_native(granit_timestamp_query_pool handle) noexcept {
    return timestamp_query_pool_ref{handle};
  }

private:
  friend class timestamp_query_pool;
  explicit constexpr timestamp_query_pool_ref(granit_timestamp_query_pool handle) noexcept
      : handle_(handle) {}
  granit_timestamp_query_pool handle_{GRANIT_NULL_HANDLE};
};

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

  [[nodiscard]] result initialize(renderer_ref owner, std::uint32_t query_count) noexcept {
    const auto renderer = owner.native_handle();
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
  [[nodiscard]] result initialize(renderer& owner, std::uint32_t query_count) noexcept {
    return initialize(owner.ref(), query_count);
  }
  [[nodiscard]] result get_results(std::uint32_t first,
                                   std::span<std::uint64_t> nanoseconds) noexcept {
    return from_native(granit_timestamp_query_pool_get_results(
        renderer_, handle_, first, static_cast<std::uint32_t>(nanoseconds.size()),
        nanoseconds.data()));
  }
  [[nodiscard]] result get_results_async(std::uint32_t first, std::uint32_t count,
                                         async_operation& operation) noexcept {
    if (operation.valid())
      return result::invalid_argument;
    granit_async_operation handle = GRANIT_NULL_HANDLE;
    const auto value =
        granit_timestamp_query_pool_get_results_async(renderer_, handle_, first, count, &handle);
    if (value == GRANIT_SUCCESS)
      operation = async_operation{renderer_ref::from_native(renderer_), handle};
    return from_native(value);
  }
  [[nodiscard]] result copy_results(const async_operation& operation,
                                    std::span<std::uint64_t> nanoseconds) noexcept {
    return from_native(granit_timestamp_query_pool_copy_results(
        renderer_, handle_, operation.native_handle(), nanoseconds.data(),
        static_cast<std::uint32_t>(nanoseconds.size())));
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_timestamp_query_pool_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr timestamp_query_pool_ref ref() const noexcept {
    return timestamp_query_pool_ref{handle_};
  }
  [[nodiscard]] granit_timestamp_query_pool native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_timestamp_query_pool handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
