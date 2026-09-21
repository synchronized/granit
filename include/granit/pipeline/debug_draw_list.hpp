// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_DEBUG_DRAW_LIST_HPP_
#define GRANIT_PIPELINE_DEBUG_DRAW_LIST_HPP_

#include <granit/core/result.hpp>
#include <granit/math/types.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/debug_draw_list.h>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/render_target.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace granit {

using debug_draw_vertex = ::granit_debug_draw_vertex;

enum class debug_draw_space : std::uint32_t {
  world = GRANIT_DEBUG_DRAW_SPACE_WORLD,
  screen = GRANIT_DEBUG_DRAW_SPACE_SCREEN,
};

enum class debug_draw_depth_mode : std::uint32_t {
  disabled = GRANIT_DEBUG_DRAW_DEPTH_MODE_DISABLED,
  test = GRANIT_DEBUG_DRAW_DEPTH_MODE_TEST,
};

struct debug_draw_line {
  debug_draw_vertex start;
  debug_draw_vertex end;
  float width{1.0F};
  debug_draw_space space{debug_draw_space::world};
  debug_draw_depth_mode depth_mode{debug_draw_depth_mode::disabled};
};

struct debug_draw_triangle {
  std::array<debug_draw_vertex, 3> vertices;
  debug_draw_space space{debug_draw_space::world};
  debug_draw_depth_mode depth_mode{debug_draw_depth_mode::disabled};
};

struct debug_draw_list_desc {
  std::uint32_t initial_line_capacity{};
  std::uint32_t initial_triangle_capacity{};
};

struct debug_draw_list_stats {
  std::uint32_t line_count{};
  std::uint32_t triangle_count{};
};

struct debug_draw_record_desc {
  texture_view_ref color;
  texture_format color_format{texture_format::undefined};
  texture_view_ref depth;
  texture_format depth_format{texture_format::undefined};
  std::uint32_t width{};
  std::uint32_t height{};
  math::matrix4 view_projection{};
  attachment_load_operation color_load_operation{attachment_load_operation::load};
  attachment_load_operation depth_load_operation{attachment_load_operation::load};
  bool encode_srgb{};
};

class debug_draw_list;

/** 不拥有 Debug Draw List，只在来源列表的有效期内使用。 */
class debug_draw_list_ref {
public:
  debug_draw_list_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_debug_draw_list native_handle() const noexcept { return handle_; }
  [[nodiscard]] static constexpr debug_draw_list_ref
  from_native(granit_debug_draw_list handle) noexcept {
    return debug_draw_list_ref{handle};
  }

private:
  friend class debug_draw_list;
  explicit constexpr debug_draw_list_ref(granit_debug_draw_list handle) noexcept
      : handle_(handle) {}
  granit_debug_draw_list handle_{GRANIT_NULL_HANDLE};
};

class debug_draw_list {
public:
  debug_draw_list() = default;
  ~debug_draw_list() { static_cast<void>(destroy()); }
  debug_draw_list(const debug_draw_list&) = delete;
  debug_draw_list& operator=(const debug_draw_list&) = delete;
  debug_draw_list(debug_draw_list&& other) noexcept { move_from(other); }
  debug_draw_list& operator=(debug_draw_list&& other) noexcept {
    if (this != &other) {
      static_cast<void>(destroy());
      move_from(other);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const granit_debug_draw_list_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_debug_draw_list_create(renderer, &desc, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result initialize(renderer& owner, const debug_draw_list_desc& desc = {}) noexcept {
    const granit_debug_draw_list_desc native{
        .struct_size = GRANIT_DEBUG_DRAW_LIST_DESC_VERSION_1_SIZE,
        .initial_line_capacity = desc.initial_line_capacity,
        .initial_triangle_capacity = desc.initial_triangle_capacity,
        .reserved = {},
    };
    return initialize(owner.native_handle(), native);
  }
  [[nodiscard]] result clear() noexcept {
    return from_native(granit_debug_draw_list_clear(renderer_, handle_));
  }
  [[nodiscard]] result append_lines(std::span<const granit_debug_draw_line> lines) noexcept {
    if (lines.size() > std::numeric_limits<uint32_t>::max())
      return result::invalid_argument;
    return from_native(granit_debug_draw_list_append_lines(renderer_, handle_, lines.data(),
                                                           static_cast<uint32_t>(lines.size())));
  }
  [[nodiscard]] result append_lines(std::span<const debug_draw_line> lines) noexcept {
    if (lines.size() > std::numeric_limits<std::uint32_t>::max())
      return result::invalid_argument;
    try {
      std::vector<granit_debug_draw_line> native;
      native.reserve(lines.size());
      for (const auto& line : lines) {
        native.push_back({.start = line.start,
                          .end = line.end,
                          .width = line.width,
                          .space = static_cast<granit_debug_draw_space>(line.space),
                          .depth_mode = static_cast<granit_debug_draw_depth_mode>(line.depth_mode),
                          .reserved = 0});
      }
      return append_lines(native);
    } catch (const std::bad_alloc&) {
      return result::out_of_memory;
    } catch (...) {
      return result::internal;
    }
  }
  [[nodiscard]] result
  append_triangles(std::span<const granit_debug_draw_triangle> triangles) noexcept {
    if (triangles.size() > std::numeric_limits<uint32_t>::max())
      return result::invalid_argument;
    return from_native(granit_debug_draw_list_append_triangles(
        renderer_, handle_, triangles.data(), static_cast<uint32_t>(triangles.size())));
  }
  [[nodiscard]] result append_triangles(std::span<const debug_draw_triangle> triangles) noexcept {
    if (triangles.size() > std::numeric_limits<std::uint32_t>::max())
      return result::invalid_argument;
    try {
      std::vector<granit_debug_draw_triangle> native;
      native.reserve(triangles.size());
      for (const auto& triangle : triangles) {
        native.push_back(
            {.vertices = {triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]},
             .space = static_cast<granit_debug_draw_space>(triangle.space),
             .depth_mode = static_cast<granit_debug_draw_depth_mode>(triangle.depth_mode),
             .reserved = {}});
      }
      return append_triangles(native);
    } catch (const std::bad_alloc&) {
      return result::out_of_memory;
    } catch (...) {
      return result::internal;
    }
  }
  [[nodiscard]] result get_stats(granit_debug_draw_list_stats& stats) const noexcept {
    return from_native(granit_debug_draw_list_get_stats(renderer_, handle_, &stats));
  }
  [[nodiscard]] result get_stats(debug_draw_list_stats& stats) const noexcept {
    granit_debug_draw_list_stats native = GRANIT_DEBUG_DRAW_LIST_STATS_INIT;
    const auto value = get_stats(native);
    if (value.ok())
      stats = {.line_count = native.line_count, .triangle_count = native.triangle_count};
    return value;
  }
  [[nodiscard]] result append_screen_to_canvas(granit_canvas_draw_list canvas) noexcept {
    return from_native(granit_debug_draw_list_append_screen_to_canvas(renderer_, handle_, canvas));
  }
  [[nodiscard]] result append_screen_to_canvas(canvas_draw_list_ref canvas) noexcept {
    return append_screen_to_canvas(canvas.native_handle());
  }
  [[nodiscard]] result record_world(granit_command_recorder recorder,
                                    const granit_debug_draw_record_desc& desc) noexcept {
    return from_native(granit_debug_draw_list_record_world(renderer_, recorder, handle_, &desc));
  }
  [[nodiscard]] result record_world(command_recorder& recorder,
                                    const debug_draw_record_desc& desc) noexcept {
    const granit_debug_draw_record_desc native{
        .struct_size = GRANIT_DEBUG_DRAW_RECORD_DESC_VERSION_1_SIZE,
        .color = desc.color.native_handle(),
        .color_format = static_cast<granit_texture_format>(desc.color_format),
        .depth = desc.depth.native_handle(),
        .depth_format = static_cast<granit_texture_format>(desc.depth_format),
        .width = desc.width,
        .height = desc.height,
        .view_projection = desc.view_projection,
        .color_load_operation =
            static_cast<granit_attachment_load_operation>(desc.color_load_operation),
        .depth_load_operation =
            static_cast<granit_attachment_load_operation>(desc.depth_load_operation),
        .encode_srgb = desc.encode_srgb ? 1U : 0U,
        .reserved = {},
    };
    return record_world(recorder.native_handle(), native);
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_debug_draw_list_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr debug_draw_list_ref ref() const noexcept {
    return debug_draw_list_ref{handle_};
  }
  [[nodiscard]] granit_debug_draw_list native_handle() const noexcept { return handle_; }

private:
  void move_from(debug_draw_list& other) noexcept {
    renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
    handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
  }
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_debug_draw_list handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
