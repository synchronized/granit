// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDER_GRAPH_TYPES_HPP_
#define GRANIT_RENDER_GRAPH_TYPES_HPP_

#include <granit/core/result.hpp>
#include <granit/render_graph/render_graph.h>
#include <granit/renderer/texture.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace granit::render_graph {

using render_graph_resource = std::uint32_t;
using render_graph_pass = std::uint32_t;
using render_graph_native_handle = granit_render_graph;

inline constexpr render_graph_resource invalid_render_graph_resource =
    GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
inline constexpr render_graph_pass invalid_render_graph_pass = GRANIT_RENDER_GRAPH_INVALID_PASS;

enum class render_graph_access : std::uint32_t {
  read = GRANIT_RENDER_GRAPH_ACCESS_READ_BIT,
  write = GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT,
  read_write = GRANIT_RENDER_GRAPH_ACCESS_READ_BIT | GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT,
};

enum class render_graph_usage : std::uint32_t {
  sampled = GRANIT_RENDER_GRAPH_USAGE_SAMPLED_BIT,
  storage = GRANIT_RENDER_GRAPH_USAGE_STORAGE_BIT,
  color_attachment = GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT,
  depth_attachment = GRANIT_RENDER_GRAPH_USAGE_DEPTH_ATTACHMENT_BIT,
  transfer_source = GRANIT_RENDER_GRAPH_USAGE_TRANSFER_SOURCE_BIT,
  transfer_destination = GRANIT_RENDER_GRAPH_USAGE_TRANSFER_DESTINATION_BIT,
};

[[nodiscard]] constexpr render_graph_usage operator|(render_graph_usage left,
                                                     render_graph_usage right) noexcept {
  return static_cast<render_graph_usage>(static_cast<std::uint32_t>(left) |
                                         static_cast<std::uint32_t>(right));
}

struct render_graph_desc {
  std::uint32_t flags{};
};

struct render_graph_access_desc {
  render_graph_resource resource{invalid_render_graph_resource};
  render_graph_access access{render_graph_access::read};
  render_graph_usage usage{render_graph_usage::sampled};
};

struct render_graph_pass_desc {
  bool side_effect{};
  std::string_view name{};
  std::span<const render_graph_access_desc> accesses{};
};

struct render_graph_transient_texture_update {
  render_graph_resource resource{invalid_render_graph_resource};
  texture_desc desc{};
};

struct render_graph_execution_result {
  result status{result::success};
  std::uint32_t phase{GRANIT_RENDER_GRAPH_EXECUTION_PHASE_NONE};
  std::uint32_t graph_error{GRANIT_RENDER_GRAPH_COMPILE_ERROR_NONE};
  render_graph_pass error_pass{invalid_render_graph_pass};
  render_graph_resource error_resource{invalid_render_graph_resource};
  std::string_view error_pass_name{};
  std::string_view error_resource_name{};
  granit_command_recorder recorder{GRANIT_NULL_HANDLE};
};

using render_graph_record_callback = granit_render_graph_record_callback;

} // namespace granit::render_graph

#endif
