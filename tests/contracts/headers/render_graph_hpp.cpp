// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/render_graph.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::render_graph_handle>);
static_assert(std::is_move_constructible_v<granit::render_graph_handle>);
static_assert(requires(granit::render_graph_handle& graph) { graph.native_handle(); });
static_assert(requires(granit::render_graph_handle& graph) { graph.initialize(); });
