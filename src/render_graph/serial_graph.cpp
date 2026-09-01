// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_graph/serial_graph.h"

#include <algorithm>
#include <new>
#include <utility>

namespace granit::render_graph {
namespace {

execution_result fail(granit_result result, execution_phase phase, pass_id pass = invalid_pass_id) {
  execution_result failure;
  failure.result = result;
  failure.phase = phase;
  failure.error_pass = pass;
  return failure;
}

void destroy_recorder(granit_renderer renderer, granit_command_recorder recorder) noexcept {
  if (recorder != GRANIT_NULL_HANDLE) {
    static_cast<void>(granit_command_recorder_destroy(renderer, recorder));
  }
}

} // namespace

pass_context::pass_context(granit_renderer renderer, granit_command_recorder recorder,
                           std::uint32_t frame_slot, std::span<const resource_access> accesses,
                           std::span<const imported_resource> resources) noexcept
    : renderer_(renderer), recorder_(recorder), frame_slot_(frame_slot), accesses_(accesses),
      resources_(resources) {}

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

resource_id serial_graph::import_buffer(granit_buffer buffer, bool exported, std::string name) {
  if (buffer == GRANIT_NULL_HANDLE) {
    return invalid_resource_id;
  }
  resources_.push_back({.type = imported_resource_type::buffer, .handle = buffer});
  resource_names_.push_back(std::move(name));
  return compiler_.add_resource({.imported = true, .exported = exported});
}

resource_id serial_graph::import_texture_view(granit_texture_view view, bool exported,
                                              std::string name) {
  if (view == GRANIT_NULL_HANDLE) {
    return invalid_resource_id;
  }
  resources_.push_back({.type = imported_resource_type::texture_view, .handle = view});
  resource_names_.push_back(std::move(name));
  return compiler_.add_resource({.imported = true, .exported = exported});
}

resource_id serial_graph::create_transient_buffer(const granit_buffer_desc& desc,
                                                  std::string name) {
  resources_.push_back(
      {.type = imported_resource_type::buffer, .transient = true, .buffer_desc = desc});
  resource_names_.push_back(std::move(name));
  return compiler_.add_resource();
}

resource_id serial_graph::create_transient_texture(const granit_texture_desc& desc,
                                                   std::string name) {
  resources_.push_back(
      {.type = imported_resource_type::texture_view, .transient = true, .texture_desc = desc});
  resource_names_.push_back(std::move(name));
  return compiler_.add_resource();
}

pass_id serial_graph::add_pass(pass_desc desc, pass_callback callback, std::string name) {
  const auto pass = compiler_.add_pass(desc);
  passes_.push_back(std::move(desc));
  callbacks_.push_back(std::move(callback));
  pass_names_.push_back(std::move(name));
  return pass;
}

bool serial_graph::add_dependency(pass_id before, pass_id after) {
  return compiler_.add_dependency(before, after);
}

execution_result serial_graph::execute(granit_renderer renderer) const {
  return execute_internal(renderer, GRANIT_NULL_HANDLE, UINT32_MAX);
}

execution_result serial_graph::execute_frame(granit_renderer renderer, granit_frame frame) const {
  if (frame == GRANIT_NULL_HANDLE) {
    return fail(GRANIT_ERROR_INVALID_HANDLE, execution_phase::submit);
  }
  return execute_internal(renderer, frame, UINT32_MAX);
}

execution_result serial_graph::execute_frame(granit_renderer renderer, granit_frame frame,
                                             std::uint32_t frame_slot) const {
  if (frame == GRANIT_NULL_HANDLE || frame_slot == UINT32_MAX)
    return fail(GRANIT_ERROR_INVALID_ARGUMENT, execution_phase::submit);
  return execute_internal(renderer, frame, frame_slot);
}

diagnostic_graph serial_graph::diagnostics() const {
  return {.compilation = compiler_.compile(),
          .pass_names = pass_names_,
          .resource_names = resource_names_};
}

execution_result serial_graph::execute_internal(granit_renderer renderer, granit_frame frame,
                                                std::uint32_t frame_slot) const {
  if (renderer == GRANIT_NULL_HANDLE) {
    return fail(GRANIT_ERROR_INVALID_HANDLE, execution_phase::create_recorder);
  }

  const auto compiled = compiler_.compile();
  if (!compiled.succeeded()) {
    auto result =
        fail(GRANIT_ERROR_INVALID_ARGUMENT, execution_phase::compile, compiled.error_pass);
    result.graph_error = compiled.error;
    result.error_resource = compiled.error_resource;
    if (compiled.error_pass < pass_names_.size()) {
      result.error_pass_name = pass_names_[compiled.error_pass];
    }
    if (compiled.error_resource < resource_names_.size()) {
      result.error_resource_name = resource_names_[compiled.error_resource];
    }
    return result;
  }
  if (compiled.execution_order.empty()) {
    return {};
  }
  const auto fail_at = [&](granit_result value, execution_phase phase, pass_id pass,
                           resource_id resource = invalid_resource_id) {
    auto failure = fail(value, phase, pass);
    failure.error_resource = resource;
    if (pass < pass_names_.size()) {
      failure.error_pass_name = pass_names_[pass];
    }
    if (resource < resource_names_.size()) {
      failure.error_resource_name = resource_names_[resource];
    }
    return failure;
  };

  auto resources = resources_;
  std::vector<granit_texture> transient_textures(resources.size(), GRANIT_NULL_HANDLE);
  granit_result result = GRANIT_SUCCESS;
  const auto destroy_resource = [&](std::size_t index) noexcept {
    auto& resource = resources[index];
    if (!resource.transient || resource.handle == GRANIT_NULL_HANDLE) {
      return GRANIT_SUCCESS;
    }
    granit_result destroy_result = GRANIT_SUCCESS;
    if (resource.type == imported_resource_type::buffer) {
      destroy_result = granit_buffer_destroy(renderer, resource.handle);
    } else {
      destroy_result = granit_texture_view_destroy(renderer, resource.handle);
      const auto texture_result = granit_texture_destroy(renderer, transient_textures[index]);
      if (destroy_result == GRANIT_SUCCESS) {
        destroy_result = texture_result;
      }
    }
    resource.handle = GRANIT_NULL_HANDLE;
    transient_textures[index] = GRANIT_NULL_HANDLE;
    return destroy_result;
  };
  const auto destroy_resources = [&]() noexcept {
    granit_result first_error = GRANIT_SUCCESS;
    for (std::size_t index = resources.size(); index > 0; --index) {
      const auto destroy_result = destroy_resource(index - 1);
      if (first_error == GRANIT_SUCCESS && destroy_result != GRANIT_SUCCESS) {
        first_error = destroy_result;
      }
    }
    return first_error;
  };

  granit_command_recorder recorder = GRANIT_NULL_HANDLE;
  const granit_command_recorder_desc recorder_desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  result = granit_command_recorder_create(renderer, &recorder_desc, &recorder);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(destroy_resources());
    return fail(result, execution_phase::create_recorder);
  }
  result = granit_command_recorder_begin(renderer, recorder);
  if (result != GRANIT_SUCCESS) {
    destroy_recorder(renderer, recorder);
    static_cast<void>(destroy_resources());
    return fail(result, execution_phase::begin_recorder);
  }

  for (std::size_t order_index = 0; order_index < compiled.execution_order.size(); ++order_index) {
    const auto pass = compiled.execution_order[order_index];
    for (std::size_t resource_index = 0; resource_index < resources.size(); ++resource_index) {
      auto& resource = resources[resource_index];
      const auto& lifetime = compiled.resource_lifetimes[resource_index];
      if (!resource.transient || !lifetime.used || lifetime.first_use != order_index) {
        continue;
      }
      if (resource.type == imported_resource_type::buffer) {
        result = granit_buffer_create(renderer, &resource.buffer_desc, &resource.handle);
      } else {
        result = granit_texture_create_with_default_view(renderer, &resource.texture_desc,
                                                         &transient_textures[resource_index],
                                                         &resource.handle);
      }
      if (result != GRANIT_SUCCESS) {
        destroy_recorder(renderer, recorder);
        static_cast<void>(destroy_resources());
        return fail_at(result, execution_phase::create_resources, pass,
                       static_cast<resource_id>(resource_index));
      }
    }
    if (!callbacks_[pass]) {
      destroy_recorder(renderer, recorder);
      static_cast<void>(destroy_resources());
      return fail_at(GRANIT_ERROR_INVALID_ARGUMENT, execution_phase::record_pass, pass);
    }
    pass_context context(renderer, recorder, frame_slot, passes_[pass].accesses, resources);
    try {
      result = callbacks_[pass](context);
    } catch (const std::bad_alloc&) {
      result = GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      result = GRANIT_ERROR_INTERNAL;
    }
    if (result != GRANIT_SUCCESS) {
      destroy_recorder(renderer, recorder);
      static_cast<void>(destroy_resources());
      return fail_at(result, execution_phase::record_pass, pass);
    }
    for (std::size_t resource_index = resources.size(); resource_index > 0; --resource_index) {
      const auto index = resource_index - 1;
      const auto& lifetime = compiled.resource_lifetimes[index];
      if (!resources[index].transient || !lifetime.used || lifetime.last_use != order_index) {
        continue;
      }
      result = destroy_resource(index);
      if (result != GRANIT_SUCCESS) {
        destroy_recorder(renderer, recorder);
        static_cast<void>(destroy_resources());
        return fail_at(result, execution_phase::destroy_resources, pass,
                       static_cast<resource_id>(index));
      }
    }
  }

  result = granit_command_recorder_end(renderer, recorder);
  if (result != GRANIT_SUCCESS) {
    destroy_recorder(renderer, recorder);
    static_cast<void>(destroy_resources());
    return fail(result, execution_phase::end_recorder);
  }
  result = frame == GRANIT_NULL_HANDLE
               ? granit_command_recorder_submit(renderer, recorder)
               : granit_command_recorder_submit_frame(renderer, recorder, frame);
  if (result != GRANIT_SUCCESS) {
    destroy_recorder(renderer, recorder);
    static_cast<void>(destroy_resources());
    return fail(result, execution_phase::submit);
  }
  execution_result success;
  success.recorder = recorder;
  return success;
}

} // namespace granit::render_graph
