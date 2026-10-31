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

using resource_id = std::uint32_t;
using pass_id = std::uint32_t;
using handle = granit_render_graph;

inline constexpr resource_id invalid_resource = GRANIT_RENDER_GRAPH_INVALID_RESOURCE;
inline constexpr pass_id invalid_pass = GRANIT_RENDER_GRAPH_INVALID_PASS;

enum class access : std::uint32_t {
  read = GRANIT_RENDER_GRAPH_ACCESS_READ_BIT,
  write = GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT,
  read_write = GRANIT_RENDER_GRAPH_ACCESS_READ_BIT | GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT,
};

enum class usage : std::uint32_t {
  sampled = GRANIT_RENDER_GRAPH_USAGE_SAMPLED_BIT,
  storage = GRANIT_RENDER_GRAPH_USAGE_STORAGE_BIT,
  color_attachment = GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT,
  depth_attachment = GRANIT_RENDER_GRAPH_USAGE_DEPTH_ATTACHMENT_BIT,
  transfer_source = GRANIT_RENDER_GRAPH_USAGE_TRANSFER_SOURCE_BIT,
  transfer_destination = GRANIT_RENDER_GRAPH_USAGE_TRANSFER_DESTINATION_BIT,
};

[[nodiscard]] constexpr usage operator|(usage left, usage right) noexcept {
  return static_cast<usage>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

struct graph_desc {
  std::uint32_t flags{};
};

struct access_desc {
  resource_id resource{invalid_resource};
  access direction{access::read};
  usage kind{usage::sampled};
};

struct pass_desc {
  bool side_effect{};
  std::string_view name{};
  std::span<const access_desc> accesses{};
};

struct transient_texture_update {
  resource_id resource{invalid_resource};
  texture_desc desc{};
};

struct execution_result {
  result status{result::success};
  std::uint32_t phase{GRANIT_RENDER_GRAPH_EXECUTION_PHASE_NONE};
  std::uint32_t graph_error{GRANIT_RENDER_GRAPH_COMPILE_ERROR_NONE};
  pass_id error_pass{invalid_pass};
  resource_id error_resource{invalid_resource};
  std::string_view error_pass_name{};
  std::string_view error_resource_name{};
  granit_command_recorder recorder{GRANIT_NULL_HANDLE};
};

using record_callback = granit_render_graph_record_callback;

} // namespace granit::render_graph

#endif
