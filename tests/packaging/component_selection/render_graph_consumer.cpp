// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/render_graph/render_graph.hpp>

int main() {
  granit::render_graph::render_graph_handle graph;
  return graph.initialize().ok() && graph.valid() ? 0 : 1;
}
