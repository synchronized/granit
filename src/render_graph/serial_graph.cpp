// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_graph/serial_graph.h"

#include <algorithm>
#include <new>
#include <utility>

namespace granit::render_graph {
namespace {

execution_result fail(granit_result result, execution_phase phase, pass_id pass = invalid_pass_id) {
  return {.result = result, .phase = phase, .error_pass = pass};
}

void destroy_recorder(granit_renderer renderer, granit_command_recorder recorder) noexcept {
  if (recorder != GRANIT_NULL_HANDLE) {
    static_cast<void>(granit_command_recorder_destroy(renderer, recorder));
  }
}

} // namespace

pass_context::pass_context(granit_renderer renderer, granit_command_recorder recorder,
                           std::span<const resource_access> accesses,
                           std::span<const imported_resource> resources) noexcept
    : renderer_(renderer), recorder_(recorder), accesses_(accesses), resources_(resources) {}

const imported_resource* pass_context::resolve(resource_id resource,
                                               imported_resource_type type) const noexcept {
  if (resource >= resources_.size()) {
    return nullptr;
  }
  const auto declared = std::ranges::any_of(
      accesses_, [resource](const resource_access& access) { return access.resource == resource; });
  if (!declared || resources_[resource].type != type) {
    return nullptr;
  }
  return &resources_[resource];
}

granit_buffer pass_context::buffer(resource_id resource) const noexcept {
  const auto* imported = resolve(resource, imported_resource_type::buffer);
  return imported == nullptr ? GRANIT_NULL_HANDLE : imported->handle;
}

granit_texture_view pass_context::texture_view(resource_id resource) const noexcept {
  const auto* imported = resolve(resource, imported_resource_type::texture_view);
  return imported == nullptr ? GRANIT_NULL_HANDLE : imported->handle;
}

resource_id serial_graph::import_buffer(granit_buffer buffer, bool exported) {
  if (buffer == GRANIT_NULL_HANDLE) {
    return invalid_resource_id;
  }
  resources_.push_back({imported_resource_type::buffer, buffer});
  return compiler_.add_resource({.imported = true, .exported = exported});
}

resource_id serial_graph::import_texture_view(granit_texture_view view, bool exported) {
  if (view == GRANIT_NULL_HANDLE) {
    return invalid_resource_id;
  }
  resources_.push_back({imported_resource_type::texture_view, view});
  return compiler_.add_resource({.imported = true, .exported = exported});
}

pass_id serial_graph::add_pass(pass_desc desc, pass_callback callback) {
  const auto pass = compiler_.add_pass(desc);
  passes_.push_back(std::move(desc));
  callbacks_.push_back(std::move(callback));
  return pass;
}

bool serial_graph::add_dependency(pass_id before, pass_id after) {
  return compiler_.add_dependency(before, after);
}

execution_result serial_graph::execute(granit_renderer renderer) const {
  if (renderer == GRANIT_NULL_HANDLE) {
    return fail(GRANIT_ERROR_INVALID_HANDLE, execution_phase::create_recorder);
  }

  const auto compiled = compiler_.compile();
  if (!compiled.succeeded()) {
    auto result =
        fail(GRANIT_ERROR_INVALID_ARGUMENT, execution_phase::compile, compiled.error_pass);
    result.graph_error = compiled.error;
    return result;
  }
  if (compiled.execution_order.empty()) {
    return {};
  }

  granit_command_recorder recorder = GRANIT_NULL_HANDLE;
  const granit_command_recorder_desc recorder_desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  auto result = granit_command_recorder_create(renderer, &recorder_desc, &recorder);
  if (result != GRANIT_SUCCESS) {
    return fail(result, execution_phase::create_recorder);
  }
  result = granit_command_recorder_begin(renderer, recorder);
  if (result != GRANIT_SUCCESS) {
    destroy_recorder(renderer, recorder);
    return fail(result, execution_phase::begin_recorder);
  }

  for (const auto pass : compiled.execution_order) {
    if (!callbacks_[pass]) {
      destroy_recorder(renderer, recorder);
      return fail(GRANIT_ERROR_INVALID_ARGUMENT, execution_phase::record_pass, pass);
    }
    pass_context context(renderer, recorder, passes_[pass].accesses, resources_);
    try {
      result = callbacks_[pass](context);
    } catch (const std::bad_alloc&) {
      result = GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      result = GRANIT_ERROR_INTERNAL;
    }
    if (result != GRANIT_SUCCESS) {
      destroy_recorder(renderer, recorder);
      return fail(result, execution_phase::record_pass, pass);
    }
  }

  result = granit_command_recorder_end(renderer, recorder);
  if (result != GRANIT_SUCCESS) {
    destroy_recorder(renderer, recorder);
    return fail(result, execution_phase::end_recorder);
  }
  result = granit_command_recorder_submit(renderer, recorder);
  if (result != GRANIT_SUCCESS) {
    destroy_recorder(renderer, recorder);
    return fail(result, execution_phase::submit);
  }
  return {.recorder = recorder};
}

} // namespace granit::render_graph
