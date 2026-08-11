// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_UPLOAD_BATCH_HPP_
#define GRANIT_UPLOAD_BATCH_HPP_

#include <cstddef>
#include <span>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/upload_batch.h>

namespace granit {

/** 无异常、move-only 的同步批量上传包装。 */
class upload_batch {
public:
  upload_batch() = default;
  ~upload_batch() { static_cast<void>(destroy()); }
  upload_batch(const upload_batch&) = delete;
  upload_batch& operator=(const upload_batch&) = delete;
  upload_batch(upload_batch&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  upload_batch& operator=(upload_batch&& other) noexcept {
    if (this != &other) {
      static_cast<void>(destroy());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer) noexcept {
    if (valid() || renderer == GRANIT_NULL_HANDLE)
      return result::invalid_argument;
    const granit_upload_batch_desc desc = GRANIT_UPLOAD_BATCH_DESC_INIT;
    const auto value = granit_upload_batch_create(renderer, &desc, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
  }
  [[nodiscard]] result write_buffer(granit_buffer buffer, std::uint64_t offset,
                                    std::span<const std::byte> data) noexcept {
    return from_native(granit_upload_batch_write_buffer(renderer_, handle_, buffer, offset,
                                                        data.data(), data.size()));
  }
  [[nodiscard]] result submit() noexcept {
    return from_native(granit_upload_batch_submit(renderer_, handle_));
  }
  [[nodiscard]] result reset() noexcept {
    return from_native(granit_upload_batch_reset(renderer_, handle_));
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid())
      return result::success;
    const auto value = granit_upload_batch_destroy(renderer_, handle_);
    if (value == GRANIT_SUCCESS || value == GRANIT_ERROR_INVALID_HANDLE) {
      renderer_ = GRANIT_NULL_HANDLE;
      handle_ = GRANIT_NULL_HANDLE;
    }
    return from_native(value);
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_upload_batch native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_upload_batch handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
