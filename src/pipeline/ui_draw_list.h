// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_UI_DRAW_LIST_H_
#define GRANIT_PIPELINE_UI_DRAW_LIST_H_

#include <granit/core/result.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/sampler.h>

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace granit::pipeline::detail {

/** H-06C 内部 UI 顶点；颜色使用 RGBA8 UNORM，避免为每个顶点保存四个 float。 */
struct ui_vertex {
  float x = 0.0F;
  float y = 0.0F;
  float u = 0.0F;
  float v = 0.0F;
  std::uint32_t color = UINT32_C(0xFFFFFFFF);
};
static_assert(sizeof(ui_vertex) == 20);

struct ui_draw_state {
  granit_texture_view texture = GRANIT_NULL_HANDLE;
  granit_sampler sampler = GRANIT_NULL_HANDLE;
  granit_scissor scissor{};
  std::uint32_t layer = 0;
};

struct ui_draw_item {
  std::uint32_t first_index = 0;
  std::uint32_t index_count = 0;
  ui_draw_state state{};
};

/** 相邻且状态兼容的 Item 合并后形成一次实际 Draw。 */
using ui_draw_batch = ui_draw_item;

class ui_draw_list {
public:
  [[nodiscard]] granit_result append(std::span<const ui_vertex> vertices,
                                     std::span<const std::uint32_t> indices,
                                     const ui_draw_state& state) noexcept {
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

  [[nodiscard]] std::span<const ui_vertex> vertices() const noexcept { return vertices_; }
  [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept { return indices_; }
  [[nodiscard]] std::span<const ui_draw_item> items() const noexcept { return items_; }
  [[nodiscard]] std::vector<ui_draw_batch> batches() const {
    std::vector<ui_draw_batch> result;
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
  std::vector<ui_vertex> vertices_;
  std::vector<std::uint32_t> indices_;
  std::vector<ui_draw_item> items_;
};

} // namespace granit::pipeline::detail

#endif
