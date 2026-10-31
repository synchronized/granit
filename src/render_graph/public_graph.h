// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDER_GRAPH_PUBLIC_GRAPH_H_
#define GRANIT_RENDER_GRAPH_PUBLIC_GRAPH_H_

#include "render_graph/serial_graph.h"

#include <granit/render_graph/render_graph.h>

#include <string>
#include <vector>

namespace granit::render_graph::detail {

struct public_pass_context {
  pass_context* inner = nullptr;
};

class public_graph {
public:
  [[nodiscard]] granit_result import_buffer(granit_buffer buffer, bool exported, std::string name,
                                            granit_render_graph_resource& resource);
  [[nodiscard]] granit_result import_texture_view(granit_texture_view view, bool exported,
                                                  std::string name,
                                                  granit_render_graph_resource& resource);
  [[nodiscard]] granit_result create_transient_buffer(const granit_buffer_desc& desc,
                                                      std::string name,
                                                      granit_render_graph_resource& resource);
  [[nodiscard]] granit_result create_transient_texture(const granit_texture_desc& desc,
                                                       std::string name,
                                                       granit_render_graph_resource& resource);
  [[nodiscard]] granit_result update_transient_texture(
      const granit_render_graph_transient_texture_update& update);
  [[nodiscard]] granit_result add_pass(const granit_render_graph_pass_desc& desc,
                                       granit_render_graph_record_callback callback,
                                       void* user_data, granit_render_graph_pass& pass);
  [[nodiscard]] granit_result add_dependency(granit_render_graph_pass before,
                                             granit_render_graph_pass after);
  [[nodiscard]] granit_result compile();
  [[nodiscard]] granit_result execute(granit_renderer renderer, granit_frame frame,
                                      std::uint32_t frame_slot,
                                      granit_render_graph_execution_result& output);

private:
  struct callback_entry {
    granit_render_graph_record_callback callback = nullptr;
    void* user_data = nullptr;
  };

  serial_graph graph_;
  std::vector<callback_entry> callbacks_;
  std::vector<bool> resource_is_buffer_;
  std::string last_error_pass_name_;
  std::string last_error_resource_name_;
};

} // namespace granit::render_graph::detail

#endif
