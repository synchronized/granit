// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/render_graph/render_graph.h>

typedef char granit_render_graph_access_size_check[
    (sizeof(granit_render_graph_access_desc) == GRANIT_RENDER_GRAPH_ACCESS_DESC_VERSION_1_SIZE)
        ? 1
        : -1];
typedef char granit_render_graph_pass_size_check[
    (sizeof(granit_render_graph_pass_desc) == GRANIT_RENDER_GRAPH_PASS_DESC_VERSION_1_SIZE) ? 1
                                                                                             : -1];

void granit_render_graph_header_check(void) {
  (void)&granit_render_graph_create;
  (void)&granit_render_graph_add_pass;
  (void)&granit_render_graph_execute;
}
