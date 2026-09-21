// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_READBACK_BATCH_HPP_
#define GRANIT_READBACK_BATCH_HPP_

#include <cstddef>
#include <cstdint>
#include <span>

#include <granit/core/result.hpp>
#include <granit/renderer/async_operation.hpp>
#include <granit/renderer/readback_batch.h>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

namespace granit {

enum class readback_layout : std::uint32_t {
  tight = GRANIT_READBACK_LAYOUT_TIGHT,
  backend = GRANIT_READBACK_LAYOUT_BACKEND,
};

enum class readback_result_type : std::uint32_t {
  buffer = GRANIT_READBACK_RESULT_TYPE_BUFFER,
  texture = GRANIT_READBACK_RESULT_TYPE_TEXTURE,
};

struct readback_batch_options {
  std::uint64_t max_result_bytes{};
  std::uint32_t max_operation_count{};
  readback_layout texture_layout{readback_layout::tight};
};

struct readback_batch_info {
  std::uint64_t result_bytes{};
  std::uint32_t operation_count{};
  std::uint64_t max_result_bytes{};
  std::uint32_t max_operation_count{};
};

struct readback_result_info {
  readback_result_type type{readback_result_type::buffer};
  std::uint64_t required_size{};
  granit_texture_format format{GRANIT_TEXTURE_FORMAT_UNDEFINED};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t depth{};
  std::uint32_t array_layer_count{};
  std::uint32_t bytes_per_row{};
  std::uint32_t rows_per_image{};
};

class readback_batch {
public:
  readback_batch() = default;
  ~readback_batch() { static_cast<void>(reset_handle()); }
  readback_batch(const readback_batch&) = delete;
  readback_batch& operator=(const readback_batch&) = delete;
  readback_batch(readback_batch&& other) noexcept
      : renderer_(other.renderer_), handle_(other.handle_) {
    other.renderer_ = GRANIT_NULL_HANDLE;
    other.handle_ = GRANIT_NULL_HANDLE;
  }
  readback_batch& operator=(readback_batch&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset_handle());
      renderer_ = other.renderer_;
      handle_ = other.handle_;
      other.renderer_ = GRANIT_NULL_HANDLE;
      other.handle_ = GRANIT_NULL_HANDLE;
    }
    return *this;
  }

  [[nodiscard]] result create(granit_renderer renderer,
                              const readback_batch_options& options = {}) noexcept {
    static_cast<void>(reset_handle());
    const granit_readback_batch_desc desc{
        GRANIT_READBACK_BATCH_DESC_VERSION_1_SIZE, 0, options.max_result_bytes,
        options.max_operation_count, static_cast<granit_readback_layout>(options.texture_layout)};
    const auto value = granit_readback_batch_create(renderer, &desc, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
  }

  [[nodiscard]] result create(renderer& owner,
                              const readback_batch_options& options = {}) noexcept {
    return create(owner.native_handle(), options);
  }

  [[nodiscard]] result read_buffer(granit_buffer buffer, std::uint64_t offset, std::uint64_t size,
                                   std::uint32_t& result_index) noexcept {
    return from_native(
        granit_readback_batch_read_buffer(renderer_, handle_, buffer, offset, size, &result_index));
  }

  [[nodiscard]] result read_texture(granit_texture texture,
                                    const texture_write_region& region,
                                    std::uint32_t& result_index) noexcept {
    const granit_texture_write_region native{.mip_level = region.mip_level,
                                             .base_array_layer = region.base_array_layer,
                                             .array_layer_count = region.array_layer_count,
                                             .aspect = static_cast<granit_texture_aspect>(region.aspect),
                                             .x = region.x,
                                             .y = region.y,
                                             .z = region.z,
                                             .width = region.width,
                                             .height = region.height,
                                             .depth = region.depth};
    return from_native(granit_readback_batch_read_texture(renderer_, handle_, texture, &native,
                                                           &result_index));
  }

  [[nodiscard]] result get_info(readback_batch_info& info) const noexcept {
    granit_readback_batch_info native = GRANIT_READBACK_BATCH_INFO_INIT;
    const auto value = granit_readback_batch_get_info(renderer_, handle_, &native);
    if (value == GRANIT_SUCCESS) {
      info = {.result_bytes = native.result_bytes,
              .operation_count = native.operation_count,
              .max_result_bytes = native.max_result_bytes,
              .max_operation_count = native.max_operation_count};
    }
    return from_native(value);
  }

  [[nodiscard]] result submit_async(async_operation& operation) noexcept {
    static_cast<void>(operation.reset());
    granit_async_operation native = GRANIT_NULL_HANDLE;
    const auto value = granit_readback_batch_submit_async(renderer_, handle_, &native);
    if (value == GRANIT_SUCCESS)
      operation = async_operation{renderer_, native};
    return from_native(value);
  }

  [[nodiscard]] result reset() noexcept {
    return from_native(granit_readback_batch_reset(renderer_, handle_));
  }

  [[nodiscard]] result reset_handle() noexcept {
    if (handle_ == GRANIT_NULL_HANDLE)
      return result::success;
    const auto renderer = renderer_;
    const auto handle = handle_;
    renderer_ = GRANIT_NULL_HANDLE;
    handle_ = GRANIT_NULL_HANDLE;
    return from_native(granit_readback_batch_destroy(renderer, handle));
  }

  [[nodiscard]] granit_readback_batch native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_readback_batch handle_{GRANIT_NULL_HANDLE};
};

[[nodiscard]] inline result get_readback_result_info(const async_operation& operation,
                                                     std::uint32_t result_index,
                                                     readback_result_info& info) noexcept {
  granit_readback_result_info native = GRANIT_READBACK_RESULT_INFO_INIT;
  const auto value = granit_readback_operation_get_result_info(
      operation.native_renderer(), operation.native_handle(), result_index, &native);
  if (value == GRANIT_SUCCESS) {
    info = {.type = static_cast<readback_result_type>(native.type),
            .required_size = native.required_size,
            .format = native.format,
            .width = native.width,
            .height = native.height,
            .depth = native.depth,
            .array_layer_count = native.array_layer_count,
            .bytes_per_row = native.bytes_per_row,
            .rows_per_image = native.rows_per_image};
  }
  return from_native(value);
}

[[nodiscard]] inline result copy_readback_result(const async_operation& operation,
                                                 std::uint32_t result_index,
                                                 std::span<std::byte> data,
                                                 std::uint64_t& required_size) noexcept {
  required_size = data.size();
  return from_native(granit_readback_operation_copy_result(operation.native_renderer(),
                                                           operation.native_handle(), result_index,
                                                           data.data(), &required_size));
}

} // namespace granit

#endif
