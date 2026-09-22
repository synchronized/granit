// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_UPLOAD_BATCH_HPP_
#define GRANIT_UPLOAD_BATCH_HPP_

#include <cstddef>
#include <span>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/async_operation.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>
#include <granit/renderer/upload_batch.h>

namespace granit {

struct upload_batch_options {
  std::uint64_t max_staged_bytes{};
  std::uint32_t max_operation_count{};
};

struct upload_batch_info {
  std::uint64_t staged_bytes{};
  std::uint32_t operation_count{};
  std::uint64_t max_staged_bytes{};
  std::uint32_t max_operation_count{};
};

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

  [[nodiscard]] result initialize(renderer_ref owner, upload_batch_options options = {}) noexcept {
    const auto renderer = owner.native_handle();
    if (valid())
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    granit_upload_batch_desc desc = GRANIT_UPLOAD_BATCH_DESC_INIT;
    desc.max_staged_bytes = options.max_staged_bytes;
    desc.max_operation_count = options.max_operation_count;
    const auto value = granit_upload_batch_create(renderer, &desc, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
  }
  [[nodiscard]] result initialize(renderer& owner, upload_batch_options options = {}) noexcept {
    return initialize(owner.ref(), options);
  }
  [[nodiscard]] result write_buffer(granit_buffer buffer, std::uint64_t offset,
                                    std::span<const std::byte> data) noexcept {
    return from_native(granit_upload_batch_write_buffer(renderer_, handle_, buffer, offset,
                                                        data.data(), data.size()));
  }
  [[nodiscard]] result write_buffer(buffer_ref buffer, std::uint64_t offset,
                                    std::span<const std::byte> data) noexcept {
    return write_buffer(buffer.native_handle(), offset, data);
  }
  [[nodiscard]] result write_texture(granit_texture texture, std::span<const std::byte> data,
                                     texture_data_layout layout,
                                     texture_write_region region) noexcept {
    const granit_texture_data_layout native_layout{.offset = layout.offset,
                                                   .bytes_per_row = layout.bytes_per_row,
                                                   .rows_per_image = layout.rows_per_image};
    const granit_texture_write_region native_region{.mip_level = region.mip_level,
                                                    .base_array_layer = region.base_array_layer,
                                                    .array_layer_count = region.array_layer_count,
                                                    .aspect =
                                                        static_cast<std::uint32_t>(region.aspect),
                                                    .x = region.x,
                                                    .y = region.y,
                                                    .z = region.z,
                                                    .width = region.width,
                                                    .height = region.height,
                                                    .depth = region.depth};
    return from_native(granit_upload_batch_write_texture(
        renderer_, handle_, texture, data.data(), data.size(), &native_layout, &native_region));
  }
  [[nodiscard]] result write_texture(texture_ref texture, std::span<const std::byte> data,
                                     texture_data_layout layout,
                                     texture_write_region region) noexcept {
    return write_texture(texture.native_handle(), data, layout, region);
  }
  [[nodiscard]] result submit() noexcept {
    return from_native(granit_upload_batch_submit(renderer_, handle_));
  }
  [[nodiscard]] result submit_async(async_operation& operation) noexcept {
    if (operation.valid())
      return result::invalid_argument;
    granit_async_operation native = GRANIT_NULL_HANDLE;
    const auto value = granit_upload_batch_submit_async(renderer_, handle_, &native);
    if (value == GRANIT_SUCCESS)
      operation = async_operation{renderer_ref::from_native(renderer_), native};
    return from_native(value);
  }
  [[nodiscard]] result get_info(upload_batch_info& info) const noexcept {
    granit_upload_batch_info native = GRANIT_UPLOAD_BATCH_INFO_INIT;
    const auto value = granit_upload_batch_get_info(renderer_, handle_, &native);
    if (value == GRANIT_SUCCESS) {
      info = {.staged_bytes = native.staged_bytes,
              .operation_count = native.operation_count,
              .max_staged_bytes = native.max_staged_bytes,
              .max_operation_count = native.max_operation_count};
    }
    return from_native(value);
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
