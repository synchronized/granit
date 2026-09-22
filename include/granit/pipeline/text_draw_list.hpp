// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_TEXT_DRAW_LIST_HPP_
#define GRANIT_PIPELINE_TEXT_DRAW_LIST_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/text_atlas.hpp>
#include <granit/pipeline/text_draw_list.h>
#include <granit/renderer/renderer.hpp>

#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace granit {

struct text_glyph_instance {
  std::uint64_t font_key{};
  std::uint32_t glyph_id{};
  std::uint32_t color{UINT32_C(0xffffffff)};
  float x{};
  float y{};
};

struct text_draw_list_desc {
  std::uint32_t initial_glyph_capacity{};
  std::uint32_t initial_run_capacity{};
};

struct text_draw_list_stats {
  std::uint32_t glyph_count{};
  std::uint32_t run_count{};
};

class text_draw_list;

/** 公共 Text Draw List C ABI 的轻量 move-only RAII 包装。 */
class text_draw_list {
public:
  text_draw_list() = default;
  ~text_draw_list() { static_cast<void>(destroy()); }
  text_draw_list(const text_draw_list&) = delete;
  text_draw_list& operator=(const text_draw_list&) = delete;
  text_draw_list(text_draw_list&& other) noexcept { move_from(other); }
  text_draw_list& operator=(text_draw_list&& other) noexcept {
    if (this != &other) {
      static_cast<void>(destroy());
      move_from(other);
    }
    return *this;
  }

  [[nodiscard]] result initialize(renderer_ref owner,
                                  const text_draw_list_desc& desc = {}) noexcept {
    const auto renderer = owner.native_handle();
    if (valid())
      return result::invalid_argument;
    const granit_text_draw_list_desc native{
        .struct_size = GRANIT_TEXT_DRAW_LIST_DESC_VERSION_1_SIZE,
        .initial_glyph_capacity = desc.initial_glyph_capacity,
        .initial_run_capacity = desc.initial_run_capacity,
        .reserved = {},
    };
    const auto value = from_native(granit_text_draw_list_create(renderer, &native, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result initialize(renderer& owner, const text_draw_list_desc& desc = {}) noexcept {
    return initialize(owner.ref(), desc);
  }
  [[nodiscard]] result clear() noexcept {
    return from_native(granit_text_draw_list_clear(renderer_, handle_));
  }
  [[nodiscard]] result append_glyph_run(std::span<const text_glyph_instance> glyphs,
                                        scissor clip = {}) noexcept {
    if (glyphs.size() > std::numeric_limits<std::uint32_t>::max())
      return result::invalid_argument;
    try {
      std::vector<granit_text_glyph_instance> native;
      native.reserve(glyphs.size());
      for (const auto& glyph : glyphs) {
        native.push_back({.font_key = glyph.font_key,
                          .glyph_id = glyph.glyph_id,
                          .color = glyph.color,
                          .x = glyph.x,
                          .y = glyph.y,
                          .reserved = {}});
      }
      const granit_text_glyph_run_desc desc{
          .struct_size = GRANIT_TEXT_GLYPH_RUN_DESC_VERSION_1_SIZE,
          .glyph_count = static_cast<std::uint32_t>(native.size()),
          .glyphs = native.data(),
          .scissor = clip,
          .reserved = {}};
      return from_native(granit_text_draw_list_append_glyph_run(renderer_, handle_, &desc));
    } catch (const std::bad_alloc&) {
      return result::out_of_memory;
    } catch (...) {
      return result::internal;
    }
  }
  [[nodiscard]] result get_stats(text_draw_list_stats& stats) const noexcept {
    granit_text_draw_list_stats native = GRANIT_TEXT_DRAW_LIST_STATS_INIT;
    const auto value =
        from_native(granit_text_draw_list_get_stats(renderer_, handle_, &native));
    if (value.ok())
      stats = {.glyph_count = native.glyph_count, .run_count = native.run_count};
    return value;
  }
  [[nodiscard]] result append_to_canvas(const text_atlas& atlas,
                                        canvas_draw_list_ref canvas) const noexcept {
    return from_native(granit_text_draw_list_append_to_canvas(
        renderer_, handle_, atlas.native_handle(), canvas.native_handle()));
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_text_draw_list_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_text_draw_list native_handle() const noexcept { return handle_; }

private:
  void move_from(text_draw_list& other) noexcept {
    renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
    handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
  }

  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_text_draw_list handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
