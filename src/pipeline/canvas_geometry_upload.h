// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_CANVAS_GEOMETRY_UPLOAD_H_
#define GRANIT_PIPELINE_CANVAS_GEOMETRY_UPLOAD_H_

#include "pipeline/canvas_draw_list.h"

#include <granit/renderer/buffer.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace granit::pipeline::detail {

/**
 * 保存一帧 Canvas 几何的可写 GPU Buffer。首版使用 Upload 内存，后续基线证明必要时再改为暂存复制。
 */
class canvas_geometry_upload {
public:
  explicit canvas_geometry_upload(std::uint32_t slot_count = GRANIT_DEFAULT_FRAMES_IN_FLIGHT)
      : slots_(slot_count), current_slot_(slots_.size() - 1) {}
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

  [[nodiscard]] granit_result
  upload(granit_renderer renderer, const canvas_draw_list& list,
         std::uint32_t frame_slot = GRANIT_CANVAS_FRAME_SLOT_AUTO) noexcept {
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
    if (frame_slot == GRANIT_CANVAS_FRAME_SLOT_AUTO)
      current_slot_ = (current_slot_ + 1) % slots_.size();
    else if (frame_slot < slots_.size())
      current_slot_ = frame_slot;
    else
      return GRANIT_ERROR_INVALID_ARGUMENT;
    auto& slot = slots_[current_slot_];
    auto result = ensure_buffer(renderer, GRANIT_BUFFER_USAGE_VERTEX_BIT,
                                static_cast<std::uint64_t>(vertex_bytes), slot.vertex,
                                slot.vertex_capacity, slot.vertex_mapping);
    if (result == GRANIT_SUCCESS) {
      result = ensure_buffer(renderer, GRANIT_BUFFER_USAGE_INDEX_BIT,
                             static_cast<std::uint64_t>(index_bytes), slot.index,
                             slot.index_capacity, slot.index_mapping);
    }
    if (result == GRANIT_SUCCESS) {
      std::memcpy(slot.vertex_mapping, list.vertices().data(), vertex_bytes);
      result = granit_buffer_flush(renderer, slot.vertex, 0, vertex_bytes);
    }
    if (result == GRANIT_SUCCESS) {
      std::memcpy(slot.index_mapping, list.indices().data(), index_bytes);
      result = granit_buffer_flush(renderer, slot.index, 0, index_bytes);
    }
    if (result != GRANIT_SUCCESS)
      return result;

    vertex_count_ = static_cast<std::uint32_t>(list.vertices().size());
    index_count_ = static_cast<std::uint32_t>(list.indices().size());
    return GRANIT_SUCCESS;
  }

  void reset() noexcept {
    if (renderer_ != GRANIT_NULL_HANDLE) {
      for (auto& slot : slots_) {
        release_buffer(renderer_, slot.index, slot.index_mapping);
        release_buffer(renderer_, slot.vertex, slot.vertex_mapping);
      }
    }
    renderer_ = GRANIT_NULL_HANDLE;
    for (auto& slot : slots_)
      slot = {};
    current_slot_ = slots_.empty() ? 0 : slots_.size() - 1;
    vertex_count_ = 0;
    index_count_ = 0;
  }

  [[nodiscard]] granit_buffer vertex_buffer() const noexcept {
    return slots_[current_slot_].vertex;
  }
  [[nodiscard]] granit_buffer index_buffer() const noexcept { return slots_[current_slot_].index; }
  [[nodiscard]] std::uint64_t vertex_capacity() const noexcept {
    return slots_[current_slot_].vertex_capacity;
  }
  [[nodiscard]] std::uint64_t index_capacity() const noexcept {
    return slots_[current_slot_].index_capacity;
  }
  [[nodiscard]] std::uint32_t vertex_count() const noexcept { return vertex_count_; }
  [[nodiscard]] std::uint32_t index_count() const noexcept { return index_count_; }
  [[nodiscard]] std::uint32_t frame_slot_count() const noexcept {
    return static_cast<std::uint32_t>(slots_.size());
  }
  [[nodiscard]] const void* vertex_data() const noexcept {
    return slots_[current_slot_].vertex_mapping;
  }
  [[nodiscard]] const void* index_data() const noexcept {
    return slots_[current_slot_].index_mapping;
  }

private:
  struct upload_slot {
    granit_buffer vertex = GRANIT_NULL_HANDLE;
    granit_buffer index = GRANIT_NULL_HANDLE;
    void* vertex_mapping = nullptr;
    void* index_mapping = nullptr;
    std::uint64_t vertex_capacity = 0;
    std::uint64_t index_capacity = 0;
  };

  static std::uint64_t next_capacity(std::uint64_t required) noexcept {
    constexpr std::uint64_t minimum = 4096;
    std::uint64_t capacity = minimum;
    while (capacity < required && capacity <= std::numeric_limits<std::uint64_t>::max() / 2)
      capacity *= 2;
    return std::max(capacity, required);
  }

  static granit_result ensure_buffer(granit_renderer renderer, granit_buffer_usage usage,
                                     std::uint64_t required, granit_buffer& buffer,
                                     std::uint64_t& capacity, void*& mapping) noexcept {
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
    void* replacement_mapping = nullptr;
    const auto map_result =
        granit_buffer_map(renderer, replacement, 0, replacement_capacity, &replacement_mapping);
    if (map_result != GRANIT_SUCCESS) {
      static_cast<void>(granit_buffer_destroy(renderer, replacement));
      return map_result;
    }
    release_buffer(renderer, buffer, mapping);
    buffer = replacement;
    mapping = replacement_mapping;
    capacity = replacement_capacity;
    return GRANIT_SUCCESS;
  }

  static void release_buffer(granit_renderer renderer, granit_buffer& buffer,
                             void*& mapping) noexcept {
    if (buffer != GRANIT_NULL_HANDLE) {
      if (mapping != nullptr)
        static_cast<void>(granit_buffer_unmap(renderer, buffer));
      static_cast<void>(granit_buffer_destroy(renderer, buffer));
    }
    buffer = GRANIT_NULL_HANDLE;
    mapping = nullptr;
  }

  void move_from(canvas_geometry_upload& other) noexcept {
    renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
    slots_ = std::move(other.slots_);
    current_slot_ = std::exchange(other.current_slot_, 0);
    vertex_count_ = std::exchange(other.vertex_count_, 0);
    index_count_ = std::exchange(other.index_count_, 0);
  }

  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  std::vector<upload_slot> slots_;
  std::size_t current_slot_ = 0;
  std::uint32_t vertex_count_ = 0;
  std::uint32_t index_count_ = 0;
};

} // namespace granit::pipeline::detail

#endif
