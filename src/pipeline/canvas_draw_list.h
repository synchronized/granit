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

struct canvas_draw_state {
  granit_texture_view texture = GRANIT_NULL_HANDLE;
  granit_sampler sampler = GRANIT_NULL_HANDLE;
  granit_scissor scissor{};
  std::uint32_t layer = 0;
};

struct canvas_draw_item {
  std::uint32_t first_index = 0;
  std::uint32_t index_count = 0;
  canvas_draw_state state{};
};

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
      return GRANIT_SUCCESS;
    } catch (...) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
  }

  [[nodiscard]] granit_result append(std::span<const canvas_vertex> vertices,
                                     std::span<const std::uint32_t> indices,
                                     const canvas_draw_state& state) noexcept {
    if (vertices.empty() || indices.empty())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (vertices_.size() > std::numeric_limits<std::uint32_t>::max() - vertices.size() ||
        indices_.size() > std::numeric_limits<std::uint32_t>::max() - indices.size()) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    for (const auto index : indices) {
      if (index >= vertices.size())
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }

    const auto vertex_base = static_cast<std::uint32_t>(vertices_.size());
    const auto first_index = static_cast<std::uint32_t>(indices_.size());
    try {
      vertices_.insert(vertices_.end(), vertices.begin(), vertices.end());
      for (const auto index : indices)
        indices_.push_back(vertex_base + index);
      items_.push_back({first_index, static_cast<std::uint32_t>(indices.size()), state});
    } catch (...) {
      vertices_.resize(vertex_base);
      indices_.resize(first_index);
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    return GRANIT_SUCCESS;
  }

  void clear() noexcept {
    vertices_.clear();
    indices_.clear();
    items_.clear();
  }

  [[nodiscard]] std::span<const canvas_vertex> vertices() const noexcept { return vertices_; }
  [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept { return indices_; }
  [[nodiscard]] std::span<const canvas_draw_item> items() const noexcept { return items_; }
  [[nodiscard]] std::vector<canvas_draw_batch> batches() const {
    std::vector<canvas_draw_batch> result;
    result.reserve(items_.size());
    for (const auto& item : items_) {
      if (!result.empty()) {
        auto& previous = result.back();
        const auto& left = previous.state;
        const auto& right = item.state;
        const bool same_scissor = left.scissor.x == right.scissor.x &&
                                  left.scissor.y == right.scissor.y &&
                                  left.scissor.width == right.scissor.width &&
                                  left.scissor.height == right.scissor.height;
        const bool same_state = left.texture == right.texture && left.sampler == right.sampler &&
                                left.layer == right.layer && same_scissor;
        if (previous.first_index + previous.index_count == item.first_index && same_state) {
          previous.index_count += item.index_count;
          continue;
        }
      }
      result.push_back(item);
    }
    return result;
  }

private:
  std::vector<canvas_vertex> vertices_;
  std::vector<std::uint32_t> indices_;
  std::vector<canvas_draw_item> items_;
};

} // namespace granit::pipeline::detail

#endif
