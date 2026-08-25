// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_DETAIL_CANVAS_DRAW_LIST_H_
#define GRANIT_PIPELINE_DETAIL_CANVAS_DRAW_LIST_H_

#include <granit/core/result.h>
#include <granit/pipeline/canvas_draw_list.h>

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace granit::pipeline::detail {

/** H-06C 内部实现直接复用公共固定布局顶点。 */
using canvas_vertex = granit_canvas_vertex;
static_assert(sizeof(canvas_vertex) == 20);

using canvas_draw_state = granit_canvas_draw_state;
using canvas_draw_item = granit_canvas_draw_range;

/** 相邻且状态兼容的 Item 合并后形成一次实际 Draw。 */
using canvas_draw_batch = canvas_draw_item;

class canvas_draw_list {
public:
  [[nodiscard]] granit_result reserve(std::size_t vertex_capacity, std::size_t index_capacity,
                                      std::size_t item_capacity) noexcept {
    try {
      vertices_.reserve(vertex_capacity);
      indices_.reserve(index_capacity);
      items_.reserve(item_capacity);
      batches_.reserve(item_capacity);
      return GRANIT_SUCCESS;
    } catch (...) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
  }

  [[nodiscard]] granit_result append(std::span<const canvas_vertex> vertices,
                                     std::span<const std::uint32_t> indices,
                                     const canvas_draw_state& state) noexcept {
    const canvas_draw_item item{0, static_cast<std::uint32_t>(indices.size()), state};
    return append_batch(vertices, indices, std::span{&item, 1});
  }

  [[nodiscard]] granit_result append_batch(std::span<const canvas_vertex> vertices,
                                           std::span<const std::uint32_t> indices,
                                           std::span<const canvas_draw_item> items) noexcept {
    if (vertices.empty() || indices.empty() || items.empty())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (vertices_.size() > std::numeric_limits<std::uint32_t>::max() - vertices.size() ||
        indices_.size() > std::numeric_limits<std::uint32_t>::max() - indices.size()) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    for (const auto index : indices) {
      if (index >= vertices.size())
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    std::uint32_t previous_end = 0;
    for (const auto& item : items) {
      if (item.index_count == 0 || item.first_index < previous_end ||
          item.first_index > indices.size() ||
          item.index_count > indices.size() - item.first_index) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
      previous_end = item.first_index + item.index_count;
    }

    const auto vertex_base = static_cast<std::uint32_t>(vertices_.size());
    const auto first_index = static_cast<std::uint32_t>(indices_.size());
    const auto first_item = items_.size();
    const auto first_batch = batches_.size();
    const auto previous_batch = batches_.empty() ? canvas_draw_batch{} : batches_.back();
    try {
      vertices_.insert(vertices_.end(), vertices.begin(), vertices.end());
      for (const auto index : indices)
        indices_.push_back(vertex_base + index);
      for (const auto& item : items) {
        auto adjusted = item;
        adjusted.first_index += first_index;
        items_.push_back(adjusted);
        if (!batches_.empty() && compatible(batches_.back(), adjusted))
          batches_.back().index_count += adjusted.index_count;
        else
          batches_.push_back(adjusted);
      }
    } catch (...) {
      vertices_.resize(vertex_base);
      indices_.resize(first_index);
      items_.resize(first_item);
      batches_.resize(first_batch);
      if (!batches_.empty())
        batches_.back() = previous_batch;
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    return GRANIT_SUCCESS;
  }

  void clear() noexcept {
    vertices_.clear();
    indices_.clear();
    items_.clear();
    batches_.clear();
  }

  [[nodiscard]] std::span<const canvas_vertex> vertices() const noexcept { return vertices_; }
  [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept { return indices_; }
  [[nodiscard]] std::span<const canvas_draw_item> items() const noexcept { return items_; }
  [[nodiscard]] std::span<const canvas_draw_batch> batches() const noexcept { return batches_; }

private:
  static bool compatible(const canvas_draw_batch& left, const canvas_draw_item& right) noexcept {
    const auto& left_state = left.state;
    const auto& right_state = right.state;
    return left.first_index + left.index_count == right.first_index &&
           left_state.texture == right_state.texture && left_state.sampler == right_state.sampler &&
           left_state.scissor.x == right_state.scissor.x &&
           left_state.scissor.y == right_state.scissor.y &&
           left_state.scissor.width == right_state.scissor.width &&
           left_state.scissor.height == right_state.scissor.height;
  }

  std::vector<canvas_vertex> vertices_;
  std::vector<std::uint32_t> indices_;
  std::vector<canvas_draw_item> items_;
  std::vector<canvas_draw_batch> batches_;
};

} // namespace granit::pipeline::detail

#endif
