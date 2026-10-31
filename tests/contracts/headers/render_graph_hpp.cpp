// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/render_graph/render_graph.hpp>

#include <span>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::render_graph::graph>);
static_assert(std::is_move_constructible_v<granit::render_graph::graph>);
static_assert(requires(granit::render_graph::graph& graph) { graph.native_handle(); });
static_assert(requires(granit::render_graph::graph& graph) { graph.initialize(); });
static_assert(std::is_same_v<decltype(granit::render_graph::graph_desc{}.flags), std::uint32_t>);
static_assert(requires {
  granit::render_graph::pass_desc{.name = "pass",
                                  .accesses = std::span<const granit::render_graph::access_desc>{}};
});
