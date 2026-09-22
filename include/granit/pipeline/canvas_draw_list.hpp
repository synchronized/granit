// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_CANVAS_DRAW_LIST_HPP_
#define GRANIT_PIPELINE_CANVAS_DRAW_LIST_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/canvas_draw_list.h>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/render_target.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/texture.hpp>

#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace granit {

using canvas_vertex = ::granit_canvas_vertex;

inline constexpr std::uint32_t canvas_frame_slot_auto = GRANIT_CANVAS_FRAME_SLOT_AUTO;

struct canvas_draw_state {
  texture_view_ref texture;
  sampler_ref sampler;
  scissor clip;
};

struct canvas_draw_range {
  std::uint32_t first_index{};
  std::uint32_t index_count{};
  canvas_draw_state state;
};

struct canvas_draw_list_desc {
  std::uint32_t initial_vertex_capacity{};
  std::uint32_t initial_index_capacity{};
  std::uint32_t initial_item_capacity{};
  std::uint32_t frame_slot_count{GRANIT_DEFAULT_FRAMES_IN_FLIGHT};
};

struct canvas_rect_desc {
  float x{};
  float y{};
  float width{};
  float height{};
  float u0{};
  float v0{};
  float u1{1.0F};
  float v1{1.0F};
  std::uint32_t color{UINT32_C(0xffffffff)};
  canvas_draw_state state;
};

struct canvas_draw_list_stats {
  std::uint32_t vertex_count{};
  std::uint32_t index_count{};
  std::uint32_t item_count{};
  std::uint32_t batch_count{};
};

struct canvas_record_desc {
  texture_view_ref color;
  texture_format color_format{texture_format::undefined};
  std::uint32_t width{};
  std::uint32_t height{};
  attachment_load_operation load_operation{attachment_load_operation::load};
  bool encode_srgb{};
  std::uint32_t frame_slot{canvas_frame_slot_auto};
};

class canvas_draw_list;

/** 不拥有 Canvas Draw List，只在来源列表的有效期内使用。 */
class canvas_draw_list_ref {
public:
  canvas_draw_list_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_canvas_draw_list native_handle() const noexcept { return handle_; }
  [[nodiscard]] static constexpr canvas_draw_list_ref
  from_native(granit_canvas_draw_list handle) noexcept {
    return canvas_draw_list_ref{handle};
  }

private:
  friend class canvas_draw_list;

  explicit constexpr canvas_draw_list_ref(granit_canvas_draw_list handle) noexcept
      : handle_(handle) {}

  granit_canvas_draw_list handle_{GRANIT_NULL_HANDLE};
};

/** 公共 Canvas Draw List C ABI 的轻量 move-only RAII 包装。 */
class canvas_draw_list {
public:
  canvas_draw_list() = default;
  ~canvas_draw_list() { static_cast<void>(destroy()); }
  canvas_draw_list(const canvas_draw_list&) = delete;
  canvas_draw_list& operator=(const canvas_draw_list&) = delete;
  canvas_draw_list(canvas_draw_list&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  canvas_draw_list& operator=(canvas_draw_list&& other) noexcept {
    if (this != &other) {
      static_cast<void>(destroy());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(renderer_ref owner,
                                  const granit_canvas_draw_list_desc& desc) noexcept {
    const auto renderer = owner.native_handle();
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_canvas_draw_list_create(renderer, &desc, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result initialize(renderer& owner,
                                  const canvas_draw_list_desc& desc = {}) noexcept {
    const granit_canvas_draw_list_desc native{
        .struct_size = GRANIT_CANVAS_DRAW_LIST_DESC_VERSION_1_SIZE,
        .initial_vertex_capacity = desc.initial_vertex_capacity,
        .initial_index_capacity = desc.initial_index_capacity,
        .initial_item_capacity = desc.initial_item_capacity,
        .frame_slot_count = desc.frame_slot_count,
        .reserved = {},
    };
    return initialize(owner.ref(), native);
  }
  [[nodiscard]] result clear() noexcept {
    return from_native(granit_canvas_draw_list_clear(renderer_, handle_));
  }
  [[nodiscard]] result append(std::span<const granit_canvas_vertex> vertices,
                              std::span<const std::uint32_t> indices,
                              const granit_canvas_draw_state& state) noexcept {
    if (vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
        indices.size() > std::numeric_limits<std::uint32_t>::max()) {
      return result::invalid_argument;
    }
    return from_native(granit_canvas_draw_list_append(
        renderer_, handle_, vertices.data(), static_cast<std::uint32_t>(vertices.size()),
        indices.data(), static_cast<std::uint32_t>(indices.size()), &state));
  }
  [[nodiscard]] result append(std::span<const canvas_vertex> vertices,
                              std::span<const std::uint32_t> indices,
                              const canvas_draw_state& state) noexcept {
    return append(vertices, indices, to_native(state));
  }

  [[nodiscard]] result append_batch(std::span<const granit_canvas_vertex> vertices,
                                    std::span<const std::uint32_t> indices,
                                    std::span<const granit_canvas_draw_range> ranges) noexcept {
    return from_native(granit_canvas_draw_list_append_batch(
        renderer_, handle_, vertices.data(), static_cast<std::uint32_t>(vertices.size()),
        indices.data(), static_cast<std::uint32_t>(indices.size()), ranges.data(),
        static_cast<std::uint32_t>(ranges.size())));
  }
  [[nodiscard]] result append_batch(std::span<const canvas_vertex> vertices,
                                    std::span<const std::uint32_t> indices,
                                    std::span<const canvas_draw_range> ranges) noexcept {
    if (vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
        indices.size() > std::numeric_limits<std::uint32_t>::max() ||
        ranges.size() > std::numeric_limits<std::uint32_t>::max()) {
      return result::invalid_argument;
    }
    try {
      std::vector<granit_canvas_draw_range> native;
      native.reserve(ranges.size());
      for (const auto& range : ranges) {
        native.push_back({.first_index = range.first_index,
                          .index_count = range.index_count,
                          .state = to_native(range.state)});
      }
      return append_batch(vertices, indices, native);
    } catch (const std::bad_alloc&) {
      return result::out_of_memory;
    } catch (...) {
      return result::internal;
    }
  }
  [[nodiscard]] result append_rect(const granit_canvas_rect_desc& desc) noexcept {
    return from_native(granit_canvas_draw_list_append_rect(renderer_, handle_, &desc));
  }
  [[nodiscard]] result append_rect(const canvas_rect_desc& desc) noexcept {
    const granit_canvas_rect_desc native{
        .struct_size = GRANIT_CANVAS_RECT_DESC_VERSION_1_SIZE,
        .x = desc.x,
        .y = desc.y,
        .width = desc.width,
        .height = desc.height,
        .u0 = desc.u0,
        .v0 = desc.v0,
        .u1 = desc.u1,
        .v1 = desc.v1,
        .color = desc.color,
        .state = to_native(desc.state),
        .reserved = {},
    };
    return append_rect(native);
  }
  [[nodiscard]] result get_stats(granit_canvas_draw_list_stats& stats) const noexcept {
    return from_native(granit_canvas_draw_list_get_stats(renderer_, handle_, &stats));
  }
  [[nodiscard]] result get_stats(canvas_draw_list_stats& stats) const noexcept {
    granit_canvas_draw_list_stats native = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
    const auto value = get_stats(native);
    if (value.ok()) {
      stats = {.vertex_count = native.vertex_count,
               .index_count = native.index_count,
               .item_count = native.item_count,
               .batch_count = native.batch_count};
    }
    return value;
  }
  [[nodiscard]] result record(granit_command_recorder recorder,
                              const granit_canvas_record_desc& desc) noexcept {
    return from_native(granit_canvas_draw_list_record(renderer_, recorder, handle_, &desc));
  }
  [[nodiscard]] result record(command_recorder& recorder, const canvas_record_desc& desc) noexcept {
    const granit_canvas_record_desc native{
        .struct_size = GRANIT_CANVAS_RECORD_DESC_VERSION_1_SIZE,
        .color = desc.color.native_handle(),
        .color_format = static_cast<granit_texture_format>(desc.color_format),
        .width = desc.width,
        .height = desc.height,
        .load_operation = static_cast<granit_attachment_load_operation>(desc.load_operation),
        .encode_srgb = desc.encode_srgb ? 1U : 0U,
        .frame_slot = desc.frame_slot,
        .reserved = {},
    };
    return record(recorder.native_handle(), native);
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_canvas_draw_list_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr canvas_draw_list_ref ref() const noexcept {
    return canvas_draw_list_ref{handle_};
  }
  [[nodiscard]] granit_canvas_draw_list native_handle() const noexcept { return handle_; }

private:
  [[nodiscard]] static constexpr granit_canvas_draw_state
  to_native(const canvas_draw_state& state) noexcept {
    return {.texture = state.texture.native_handle(),
            .sampler = state.sampler.native_handle(),
            .scissor = state.clip};
  }

  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_canvas_draw_list handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
