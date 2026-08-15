// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_CANVAS_GEOMETRY_UPLOAD_H_
#define GRANIT_PIPELINE_CANVAS_GEOMETRY_UPLOAD_H_

#include "pipeline/canvas_draw_list.h"

#include <granit/renderer/buffer.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace granit::pipeline::detail {

/**
 * 保存一帧 Canvas 几何的可写 GPU Buffer。首版使用 Upload 内存，后续基线证明必要时再改为暂存复制。
 */
class canvas_geometry_upload {
public:
  canvas_geometry_upload() = default;
  ~canvas_geometry_upload() { reset(); }

  canvas_geometry_upload(const canvas_geometry_upload&) = delete;
  canvas_geometry_upload& operator=(const canvas_geometry_upload&) = delete;

  canvas_geometry_upload(canvas_geometry_upload&& other) noexcept { move_from(other); }
  canvas_geometry_upload& operator=(canvas_geometry_upload&& other) noexcept {
    if (this != &other) {
      reset();
      move_from(other);
    }
    return *this;
  }

  [[nodiscard]] granit_result upload(granit_renderer renderer,
                                     const canvas_draw_list& list) noexcept {
    if (renderer == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (renderer_ != GRANIT_NULL_HANDLE && renderer_ != renderer)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto vertex_bytes = list.vertices().size_bytes();
    const auto index_bytes = list.indices().size_bytes();
    if (vertex_bytes == 0 || index_bytes == 0) {
      vertex_count_ = 0;
      index_count_ = 0;
      return GRANIT_SUCCESS;
    }
    if (vertex_bytes > std::numeric_limits<std::uint64_t>::max() ||
        index_bytes > std::numeric_limits<std::uint64_t>::max()) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    // 先记录所属 Renderer，保证部分创建失败后析构仍能回收已经创建的 Buffer。
    renderer_ = renderer;
    auto result =
        ensure_buffer(renderer, GRANIT_BUFFER_USAGE_VERTEX_BIT,
                      static_cast<std::uint64_t>(vertex_bytes), vertex_, vertex_capacity_);
    if (result == GRANIT_SUCCESS) {
      result = ensure_buffer(renderer, GRANIT_BUFFER_USAGE_INDEX_BIT,
                             static_cast<std::uint64_t>(index_bytes), index_, index_capacity_);
    }
    if (result == GRANIT_SUCCESS) {
      result = granit_buffer_write(renderer, vertex_, 0, list.vertices().data(), vertex_bytes);
    }
    if (result == GRANIT_SUCCESS)
      result = granit_buffer_write(renderer, index_, 0, list.indices().data(), index_bytes);
    if (result != GRANIT_SUCCESS)
      return result;

    vertex_count_ = static_cast<std::uint32_t>(list.vertices().size());
    index_count_ = static_cast<std::uint32_t>(list.indices().size());
    return GRANIT_SUCCESS;
  }

  void reset() noexcept {
    if (renderer_ != GRANIT_NULL_HANDLE) {
      if (index_ != GRANIT_NULL_HANDLE)
        static_cast<void>(granit_buffer_destroy(renderer_, index_));
      if (vertex_ != GRANIT_NULL_HANDLE)
        static_cast<void>(granit_buffer_destroy(renderer_, vertex_));
    }
    renderer_ = GRANIT_NULL_HANDLE;
    vertex_ = GRANIT_NULL_HANDLE;
    index_ = GRANIT_NULL_HANDLE;
    vertex_capacity_ = 0;
    index_capacity_ = 0;
    vertex_count_ = 0;
    index_count_ = 0;
  }

  [[nodiscard]] granit_buffer vertex_buffer() const noexcept { return vertex_; }
  [[nodiscard]] granit_buffer index_buffer() const noexcept { return index_; }
  [[nodiscard]] std::uint64_t vertex_capacity() const noexcept { return vertex_capacity_; }
  [[nodiscard]] std::uint64_t index_capacity() const noexcept { return index_capacity_; }
  [[nodiscard]] std::uint32_t vertex_count() const noexcept { return vertex_count_; }
  [[nodiscard]] std::uint32_t index_count() const noexcept { return index_count_; }

private:
  static std::uint64_t next_capacity(std::uint64_t required) noexcept {
    constexpr std::uint64_t minimum = 4096;
    std::uint64_t capacity = minimum;
    while (capacity < required && capacity <= std::numeric_limits<std::uint64_t>::max() / 2)
      capacity *= 2;
    return std::max(capacity, required);
  }

  static granit_result ensure_buffer(granit_renderer renderer, granit_buffer_usage usage,
                                     std::uint64_t required, granit_buffer& buffer,
                                     std::uint64_t& capacity) noexcept {
    if (buffer != GRANIT_NULL_HANDLE && capacity >= required)
      return GRANIT_SUCCESS;
    const auto replacement_capacity = next_capacity(required);
    granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
    desc.usage = usage;
    desc.memory_location = GRANIT_MEMORY_LOCATION_UPLOAD;
    desc.size = replacement_capacity;
    granit_buffer replacement = GRANIT_NULL_HANDLE;
    const auto result = granit_buffer_create(renderer, &desc, &replacement);
    if (result != GRANIT_SUCCESS)
      return result;
    if (buffer != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_buffer_destroy(renderer, buffer));
    buffer = replacement;
    capacity = replacement_capacity;
    return GRANIT_SUCCESS;
  }

  void move_from(canvas_geometry_upload& other) noexcept {
    renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
    vertex_ = std::exchange(other.vertex_, GRANIT_NULL_HANDLE);
    index_ = std::exchange(other.index_, GRANIT_NULL_HANDLE);
    vertex_capacity_ = std::exchange(other.vertex_capacity_, 0);
    index_capacity_ = std::exchange(other.index_capacity_, 0);
    vertex_count_ = std::exchange(other.vertex_count_, 0);
    index_count_ = std::exchange(other.index_count_, 0);
  }

  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_buffer vertex_ = GRANIT_NULL_HANDLE;
  granit_buffer index_ = GRANIT_NULL_HANDLE;
  std::uint64_t vertex_capacity_ = 0;
  std::uint64_t index_capacity_ = 0;
  std::uint32_t vertex_count_ = 0;
  std::uint32_t index_count_ = 0;
};

} // namespace granit::pipeline::detail

#endif
