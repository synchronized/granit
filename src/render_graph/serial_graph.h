// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDER_GRAPH_SERIAL_GRAPH_H
#define GRANIT_RENDER_GRAPH_SERIAL_GRAPH_H

#include <granit/core/result.h>
#include <granit/renderer/buffer.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/texture.h>

#include <functional>
#include <span>
#include <string>
#include <vector>

#include "render_graph/graph_compiler.h"

namespace granit::render_graph {

enum class imported_resource_type : std::uint8_t {
  buffer,
  texture_view,
};

struct imported_resource {
  imported_resource_type type = imported_resource_type::buffer;
  granit_handle handle = GRANIT_NULL_HANDLE;
  bool transient = false;
  granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
};

class pass_context {
public:
  [[nodiscard]] granit_renderer renderer() const noexcept { return renderer_; }
  [[nodiscard]] granit_command_recorder recorder() const noexcept { return recorder_; }
  [[nodiscard]] granit_buffer buffer(resource_id resource) const noexcept;
  [[nodiscard]] granit_texture_view texture_view(resource_id resource) const noexcept;

private:
  friend class serial_graph;

  pass_context(granit_renderer renderer, granit_command_recorder recorder,
               std::span<const resource_access> accesses,
               std::span<const imported_resource> resources) noexcept;
  [[nodiscard]] const imported_resource* resolve(resource_id resource,
                                                 imported_resource_type type) const noexcept;

  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_command_recorder recorder_ = GRANIT_NULL_HANDLE;
  std::span<const resource_access> accesses_;
  std::span<const imported_resource> resources_;
};

using pass_callback = std::function<granit_result(pass_context&)>;

enum class execution_phase : std::uint8_t {
  none,
  compile,
  create_recorder,
  begin_recorder,
  record_pass,
  end_recorder,
  submit,
  create_resources,
  destroy_resources,
};

struct execution_result {
  granit_result result = GRANIT_SUCCESS;
  execution_phase phase = execution_phase::none;
  compile_error graph_error = compile_error::none;
  pass_id error_pass = invalid_pass_id;
  resource_id error_resource = invalid_resource_id;
  std::string error_pass_name;
  std::string error_resource_name;
  granit_command_recorder recorder = GRANIT_NULL_HANDLE;

  [[nodiscard]] bool succeeded() const noexcept { return result == GRANIT_SUCCESS; }
};

struct diagnostic_graph {
  compile_result compilation;
  std::vector<std::string> pass_names;
  std::vector<std::string> resource_names;
};

class serial_graph {
public:
  [[nodiscard]] resource_id import_buffer(granit_buffer buffer, bool exported = false,
                                          std::string name = {});
  [[nodiscard]] resource_id import_texture_view(granit_texture_view view, bool exported = false,
                                                std::string name = {});
  [[nodiscard]] resource_id create_transient_buffer(const granit_buffer_desc& desc,
                                                    std::string name = {});
  [[nodiscard]] resource_id create_transient_texture(const granit_texture_desc& desc,
                                                     std::string name = {});
  [[nodiscard]] pass_id add_pass(pass_desc desc, pass_callback callback, std::string name = {});
  [[nodiscard]] bool add_dependency(pass_id before, pass_id after);
  [[nodiscard]] execution_result execute(granit_renderer renderer) const;
  [[nodiscard]] execution_result execute_frame(granit_renderer renderer, granit_frame frame) const;
  [[nodiscard]] diagnostic_graph diagnostics() const;

private:
  [[nodiscard]] execution_result execute_internal(granit_renderer renderer,
                                                  granit_frame frame) const;

  graph_compiler compiler_;
  std::vector<imported_resource> resources_;
  std::vector<pass_desc> passes_;
  std::vector<pass_callback> callbacks_;
  std::vector<std::string> resource_names_;
  std::vector<std::string> pass_names_;
};

} // namespace granit::render_graph

#endif
