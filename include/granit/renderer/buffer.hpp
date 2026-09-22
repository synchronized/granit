// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BUFFER_HPP_
#define GRANIT_BUFFER_HPP_

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/buffer.h>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/resource_types.hpp>

namespace granit {

class buffer;

/** 不拥有 Buffer，只在来源 Buffer 的有效期内使用。 */
class buffer_ref {
public:
  buffer_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_buffer native_handle() const noexcept { return handle_; }
  [[nodiscard]] static constexpr buffer_ref from_native(granit_buffer handle) noexcept {
    return buffer_ref{handle};
  }

private:
  friend class buffer;

  explicit constexpr buffer_ref(granit_buffer handle) noexcept : handle_(handle) {}

  granit_buffer handle_{GRANIT_NULL_HANDLE};
};

struct buffer_desc {
  std::uint64_t size{};
  buffer_usage usage{};
  memory_location location{memory_location::automatic};
};

/** 无异常、move-only 的 Buffer RAII 包装。 */
class buffer {
public:
  buffer() = default;
  ~buffer() { static_cast<void>(reset()); }

  buffer(const buffer&) = delete;
  buffer& operator=(const buffer&) = delete;

  buffer(buffer&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}

  buffer& operator=(buffer&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(renderer_ref owner, const buffer_desc& desc) noexcept {
    const auto renderer = owner.native_handle();
    if (valid()) {
      return result::invalid_argument;
    }
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_buffer_desc native_desc{
        .struct_size = GRANIT_BUFFER_DESC_VERSION_1_SIZE,
        .usage = static_cast<granit_buffer_usage>(desc.usage),
        .memory_location = static_cast<granit_memory_location>(desc.location),
        .reserved = 0,
        .size = desc.size,
        .reserved_2 = 0,
    };
    const auto native_result = granit_buffer_create(renderer, &native_desc, &handle_);
    if (native_result == GRANIT_SUCCESS) {
      renderer_ = renderer;
    }
    return from_native(native_result);
  }

  [[nodiscard]] result initialize(renderer& owner, const buffer_desc& desc) noexcept {
    return initialize(owner.ref(), desc);
  }

  [[nodiscard]] result initialize(renderer_ref owner, const buffer_desc& desc,
                                  std::span<const std::byte> initial_data) noexcept {
    const auto renderer = owner.native_handle();
    if (valid()) {
      return result::invalid_argument;
    }
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_buffer_desc native_desc{
        .struct_size = GRANIT_BUFFER_DESC_VERSION_1_SIZE,
        .usage = static_cast<granit_buffer_usage>(desc.usage),
        .memory_location = static_cast<granit_memory_location>(desc.location),
        .reserved = 0,
        .size = desc.size,
        .reserved_2 = 0,
    };
    const granit_buffer_initial_data native_data{
        .struct_size = GRANIT_BUFFER_INITIAL_DATA_VERSION_1_SIZE,
        .reserved = 0,
        .data = initial_data.data(),
        .size = initial_data.size(),
    };
    const auto native_result =
        granit_buffer_create_with_data(renderer, &native_desc, &native_data, &handle_);
    if (native_result == GRANIT_SUCCESS) {
      renderer_ = renderer;
    }
    return from_native(native_result);
  }

  [[nodiscard]] result initialize(renderer& owner, const buffer_desc& desc,
                                  std::span<const std::byte> initial_data) noexcept {
    return initialize(owner.ref(), desc, initial_data);
  }

  [[nodiscard]] result write(std::uint64_t offset, std::span<const std::byte> data) noexcept {
    return from_native(granit_buffer_write(renderer_, handle_, offset, data.data(), data.size()));
  }

  [[nodiscard]] result map(std::uint64_t offset, std::uint64_t size, void** data) noexcept {
    return from_native(granit_buffer_map(renderer_, handle_, offset, size, data));
  }

  [[nodiscard]] result unmap() noexcept {
    return from_native(granit_buffer_unmap(renderer_, handle_));
  }

  [[nodiscard]] result flush(std::uint64_t offset, std::uint64_t size) noexcept {
    return from_native(granit_buffer_flush(renderer_, handle_, offset, size));
  }

  [[nodiscard]] result reset() noexcept {
    if (!valid()) {
      return result::success;
    }
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_buffer_destroy(renderer, handle));
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr buffer_ref ref() const noexcept { return buffer_ref{handle_}; }
  [[nodiscard]] granit_buffer native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_buffer handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
