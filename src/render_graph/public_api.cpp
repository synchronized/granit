// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_graph/public_graph.h"

#include "core/handle_table.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

using granit::detail::handle_table;
using granit::detail::handle_type;
using granit::render_graph::detail::public_graph;

class graph_registry {
public:
  static graph_registry& instance() {
    static graph_registry registry;
    return registry;
  }

  granit_handle create() {
    std::lock_guard lock(mutex_);
    auto graph = std::make_unique<public_graph>();
    auto* address = graph.get();
    const auto handle = handles_.insert(address, handle_type::render_graph, 0);
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_NULL_HANDLE;
    graphs_.push_back(std::move(graph));
    return handle;
  }

  public_graph* find(granit_render_graph handle) {
    std::lock_guard lock(mutex_);
    return static_cast<public_graph*>(handles_.find(handle, handle_type::render_graph, 0));
  }

  granit_result destroy(granit_render_graph handle) {
    std::lock_guard lock(mutex_);
    void* address = nullptr;
    const auto result = handles_.erase(handle, handle_type::render_graph, 0, &address);
    if (result != GRANIT_SUCCESS)
      return result;
    const auto found = std::find_if(graphs_.begin(), graphs_.end(),
                                    [address](const auto& graph) { return graph.get() == address; });
    if (found == graphs_.end())
      return GRANIT_ERROR_INTERNAL;
    graphs_.erase(found);
    return GRANIT_SUCCESS;
  }

private:
  std::mutex mutex_;
  handle_table handles_;
  std::vector<std::unique_ptr<public_graph>> graphs_;
};

std::string copy_name(const char* name, std::uint32_t length) {
  if (length == 0)
    return {};
  if (name == nullptr)
    return {};
  return {name, name + length};
}

granit_result find_graph(granit_render_graph handle, public_graph*& graph) {
  graph = graph_registry::instance().find(handle);
  return graph == nullptr ? GRANIT_ERROR_INVALID_HANDLE : GRANIT_SUCCESS;
}

} // namespace

namespace granit::render_graph::detail {

granit_result public_graph::import_buffer(granit_buffer buffer, bool exported, std::string name,
                                          granit_render_graph_resource& resource) {
  const auto id = graph_.import_buffer(buffer, exported, std::move(name));
  if (id == invalid_resource_id)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  resource_is_buffer_.push_back(true);
  resource = id;
  return GRANIT_SUCCESS;
}

granit_result public_graph::import_texture_view(granit_texture_view view, bool exported,
                                                std::string name,
                                                granit_render_graph_resource& resource) {
  const auto id = graph_.import_texture_view(view, exported, std::move(name));
  if (id == invalid_resource_id)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  resource_is_buffer_.push_back(false);
  resource = id;
  return GRANIT_SUCCESS;
}

granit_result public_graph::create_transient_buffer(const granit_buffer_desc& desc,
                                                    std::string name,
                                                    granit_render_graph_resource& resource) {
  const auto id = graph_.create_transient_buffer(desc, std::move(name));
  if (id == invalid_resource_id)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  resource_is_buffer_.push_back(true);
  resource = id;
  return GRANIT_SUCCESS;
}

granit_result public_graph::create_transient_texture(const granit_texture_desc& desc,
                                                     std::string name,
                                                     granit_render_graph_resource& resource) {
  const auto id = graph_.create_transient_texture(desc, std::move(name));
  if (id == invalid_resource_id)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  resource_is_buffer_.push_back(false);
  resource = id;
  return GRANIT_SUCCESS;
}

granit_result public_graph::update_transient_texture(
    const granit_render_graph_transient_texture_update& update) {
  return graph_.update_transient_texture(update.resource, update.desc) ? GRANIT_SUCCESS
                                                                         : GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result public_graph::add_pass(const granit_render_graph_pass_desc& desc,
                                     granit_render_graph_record_callback callback, void* user_data,
                                     granit_render_graph_pass& pass) {
  if (callback == nullptr || (desc.access_count != 0 && desc.accesses == nullptr))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (desc.name_length != 0 && desc.name == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  pass_desc internal;
  internal.side_effect = (desc.flags & GRANIT_RENDER_GRAPH_PASS_SIDE_EFFECT_BIT) != 0;
  internal.accesses.reserve(desc.access_count);
  for (std::uint32_t index = 0; index < desc.access_count; ++index) {
    const auto& access = desc.accesses[index];
    constexpr auto known_usage = GRANIT_RENDER_GRAPH_USAGE_SAMPLED_BIT |
                                 GRANIT_RENDER_GRAPH_USAGE_STORAGE_BIT |
                                 GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT |
                                 GRANIT_RENDER_GRAPH_USAGE_DEPTH_ATTACHMENT_BIT |
                                 GRANIT_RENDER_GRAPH_USAGE_TRANSFER_SOURCE_BIT |
                                 GRANIT_RENDER_GRAPH_USAGE_TRANSFER_DESTINATION_BIT;
    constexpr auto texture_only_usage = GRANIT_RENDER_GRAPH_USAGE_SAMPLED_BIT |
                                        GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT |
                                        GRANIT_RENDER_GRAPH_USAGE_DEPTH_ATTACHMENT_BIT;
    constexpr auto read_usages = GRANIT_RENDER_GRAPH_USAGE_SAMPLED_BIT |
                                  GRANIT_RENDER_GRAPH_USAGE_TRANSFER_SOURCE_BIT;
    constexpr auto write_usages = GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT |
                                   GRANIT_RENDER_GRAPH_USAGE_DEPTH_ATTACHMENT_BIT |
                                   GRANIT_RENDER_GRAPH_USAGE_TRANSFER_DESTINATION_BIT;
    const auto reads = (access.access & GRANIT_RENDER_GRAPH_ACCESS_READ_BIT) != 0;
    const auto writes = (access.access & GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT) != 0;
    if (access.struct_size < GRANIT_RENDER_GRAPH_ACCESS_DESC_VERSION_1_SIZE ||
        access.resource == GRANIT_RENDER_GRAPH_INVALID_RESOURCE || access.usage == 0 ||
        access.resource >= resource_is_buffer_.size() ||
        (resource_is_buffer_[access.resource] && (access.usage & texture_only_usage) != 0) ||
        ((access.usage & read_usages) != 0 && !reads) ||
        ((access.usage & write_usages) != 0 && !writes) ||
        (access.usage & ~known_usage) != 0 ||
        (access.access & (GRANIT_RENDER_GRAPH_ACCESS_READ_BIT |
                          GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT)) == 0 ||
        (access.access & ~(GRANIT_RENDER_GRAPH_ACCESS_READ_BIT |
                           GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT)) != 0) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    access_type type = access_type::read;
    const auto direction = access.access &
                           (GRANIT_RENDER_GRAPH_ACCESS_READ_BIT |
                            GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT);
    if (direction == (GRANIT_RENDER_GRAPH_ACCESS_READ_BIT |
                      GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT)) {
      type = access_type::read_write;
    } else if (direction == GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT) {
      type = access_type::write;
    }
    internal.accesses.push_back({access.resource, type, access.usage});
  }

  callbacks_.push_back({callback, user_data});
  const auto callback_index = callbacks_.size() - 1;
  const auto id = graph_.add_pass(
      std::move(internal),
      [this, callback_index](pass_context& context) {
        public_pass_context public_context{&context};
        return callbacks_[callback_index].callback(
            reinterpret_cast<const granit_render_graph_pass_context*>(&public_context),
            callbacks_[callback_index].user_data);
      },
      copy_name(desc.name, desc.name_length));
  if (id == invalid_pass_id)
    return GRANIT_ERROR_INTERNAL;
  pass = id;
  return GRANIT_SUCCESS;
}

granit_result public_graph::add_dependency(granit_render_graph_pass before,
                                           granit_render_graph_pass after) {
  return graph_.add_dependency(before, after) ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result public_graph::compile() {
  return graph_.diagnostics().compilation.succeeded() ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result public_graph::execute(granit_renderer renderer, granit_frame frame,
                                    std::uint32_t frame_slot,
                                    granit_render_graph_execution_result& output) {
  const auto result = frame == GRANIT_NULL_HANDLE
                          ? graph_.execute(renderer)
                          : frame_slot == UINT32_MAX
                                ? graph_.execute_frame(renderer, frame)
                                : graph_.execute_frame(renderer, frame, frame_slot);
  last_error_pass_name_ = result.error_pass_name;
  last_error_resource_name_ = result.error_resource_name;
  output.result = result.result;
  output.phase = static_cast<std::uint32_t>(result.phase);
  output.graph_error = static_cast<std::uint32_t>(result.graph_error);
  output.error_pass = result.error_pass;
  output.error_resource = result.error_resource;
  output.error_pass_name = last_error_pass_name_.c_str();
  output.error_pass_name_length = static_cast<std::uint32_t>(last_error_pass_name_.size());
  output.error_resource_name = last_error_resource_name_.c_str();
  output.error_resource_name_length = static_cast<std::uint32_t>(last_error_resource_name_.size());
  output.recorder = result.recorder;
  return result.result;
}

} // namespace granit::render_graph::detail

extern "C" granit_result granit_render_graph_create(const granit_render_graph_desc* desc,
                                                     granit_render_graph* graph) {
  if (graph == nullptr || (desc != nullptr && desc->struct_size < GRANIT_RENDER_GRAPH_DESC_VERSION_1_SIZE))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *graph = graph_registry::instance().create();
  return *graph == GRANIT_NULL_HANDLE ? GRANIT_ERROR_OUT_OF_MEMORY : GRANIT_SUCCESS;
}

extern "C" granit_result granit_render_graph_destroy(granit_render_graph graph) {
  return graph_registry::instance().destroy(graph);
}

extern "C" granit_result granit_render_graph_import_buffer(
    granit_render_graph graph_handle, granit_buffer buffer, uint32_t exported, const char* name,
    uint32_t name_length, granit_render_graph_resource* resource) {
  if (resource == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  public_graph* graph = nullptr;
  const auto result = find_graph(graph_handle, graph);
  if (result != GRANIT_SUCCESS)
    return result;
  return graph->import_buffer(buffer, exported != 0, copy_name(name, name_length), *resource);
}

extern "C" granit_result granit_render_graph_import_texture_view(
    granit_render_graph graph_handle, granit_texture_view view, uint32_t exported, const char* name,
    uint32_t name_length, granit_render_graph_resource* resource) {
  if (resource == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  public_graph* graph = nullptr;
  const auto result = find_graph(graph_handle, graph);
  if (result != GRANIT_SUCCESS)
    return result;
  return graph->import_texture_view(view, exported != 0, copy_name(name, name_length), *resource);
}

extern "C" granit_result granit_render_graph_create_transient_buffer(
    granit_render_graph graph_handle, const granit_buffer_desc* desc, const char* name,
    uint32_t name_length, granit_render_graph_resource* resource) {
  if (desc == nullptr || resource == nullptr || desc->struct_size < GRANIT_BUFFER_DESC_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  public_graph* graph = nullptr;
  const auto result = find_graph(graph_handle, graph);
  if (result != GRANIT_SUCCESS)
    return result;
  return graph->create_transient_buffer(*desc, copy_name(name, name_length), *resource);
}

extern "C" granit_result granit_render_graph_create_transient_texture(
    granit_render_graph graph_handle, const granit_texture_desc* desc, const char* name,
    uint32_t name_length, granit_render_graph_resource* resource) {
  if (desc == nullptr || resource == nullptr || desc->struct_size < GRANIT_TEXTURE_DESC_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  public_graph* graph = nullptr;
  const auto result = find_graph(graph_handle, graph);
  if (result != GRANIT_SUCCESS)
    return result;
  return graph->create_transient_texture(*desc, copy_name(name, name_length), *resource);
}

extern "C" granit_result granit_render_graph_update_transient_texture(
    granit_render_graph graph_handle, const granit_render_graph_transient_texture_update* update) {
  if (update == nullptr || update->struct_size < GRANIT_RENDER_GRAPH_TRANSIENT_TEXTURE_UPDATE_VERSION_1_SIZE ||
      update->desc.struct_size < GRANIT_TEXTURE_DESC_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  public_graph* graph = nullptr;
  const auto result = find_graph(graph_handle, graph);
  return result == GRANIT_SUCCESS ? graph->update_transient_texture(*update) : result;
}

extern "C" granit_result granit_render_graph_add_pass(
    granit_render_graph graph_handle, const granit_render_graph_pass_desc* desc,
    granit_render_graph_record_callback callback, void* user_data,
    granit_render_graph_pass* pass) {
  if (desc == nullptr || pass == nullptr || desc->struct_size < GRANIT_RENDER_GRAPH_PASS_DESC_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  public_graph* graph = nullptr;
  const auto result = find_graph(graph_handle, graph);
  return result == GRANIT_SUCCESS ? graph->add_pass(*desc, callback, user_data, *pass) : result;
}

extern "C" granit_result granit_render_graph_add_dependency(
    granit_render_graph graph_handle, granit_render_graph_pass before,
    granit_render_graph_pass after) {
  public_graph* graph = nullptr;
  const auto result = find_graph(graph_handle, graph);
  return result == GRANIT_SUCCESS ? graph->add_dependency(before, after) : result;
}

extern "C" granit_result granit_render_graph_compile(granit_render_graph graph_handle) {
  public_graph* graph = nullptr;
  const auto result = find_graph(graph_handle, graph);
  return result == GRANIT_SUCCESS ? graph->compile() : result;
}

extern "C" granit_result granit_render_graph_execute(
    granit_render_graph graph_handle, granit_renderer renderer, granit_frame frame,
    uint32_t frame_slot, granit_render_graph_execution_result* result) {
  if (result == nullptr || result->struct_size < GRANIT_RENDER_GRAPH_EXECUTION_RESULT_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto struct_size = result->struct_size;
  *result = GRANIT_RENDER_GRAPH_EXECUTION_RESULT_INIT;
  result->struct_size = struct_size;
  public_graph* graph = nullptr;
  const auto lookup = find_graph(graph_handle, graph);
  return lookup == GRANIT_SUCCESS ? graph->execute(renderer, frame, frame_slot, *result) : lookup;
}

extern "C" granit_renderer granit_render_graph_context_renderer(
    const granit_render_graph_pass_context* context) {
  const auto* public_context = reinterpret_cast<const granit::render_graph::detail::public_pass_context*>(context);
  return public_context == nullptr || public_context->inner == nullptr ? GRANIT_NULL_HANDLE
                                                                         : public_context->inner->renderer();
}

extern "C" granit_command_recorder granit_render_graph_context_recorder(
    const granit_render_graph_pass_context* context) {
  const auto* public_context = reinterpret_cast<const granit::render_graph::detail::public_pass_context*>(context);
  return public_context == nullptr || public_context->inner == nullptr ? GRANIT_NULL_HANDLE
                                                                         : public_context->inner->recorder();
}

extern "C" granit_buffer granit_render_graph_context_buffer(
    const granit_render_graph_pass_context* context, granit_render_graph_resource resource) {
  const auto* public_context = reinterpret_cast<const granit::render_graph::detail::public_pass_context*>(context);
  return public_context == nullptr || public_context->inner == nullptr ? GRANIT_NULL_HANDLE
                                                                         : public_context->inner->buffer(resource);
}

extern "C" granit_texture_view granit_render_graph_context_texture_view(
    const granit_render_graph_pass_context* context, granit_render_graph_resource resource) {
  const auto* public_context = reinterpret_cast<const granit::render_graph::detail::public_pass_context*>(context);
  return public_context == nullptr || public_context->inner == nullptr
             ? GRANIT_NULL_HANDLE
             : public_context->inner->texture_view(resource);
}
