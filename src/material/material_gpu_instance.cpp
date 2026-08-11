// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_gpu_instance.h"

#include <granit/renderer/buffer.h>

#include <algorithm>
#include <new>

namespace granit::material {

material_gpu_instance::~material_gpu_instance() { static_cast<void>(reset()); }

granit_result material_gpu_instance::initialize(granit_renderer renderer,
                                                granit_bind_group_layout layout,
                                                const material_metadata& metadata) {
  if (renderer_ != GRANIT_NULL_HANDLE || renderer == GRANIT_NULL_HANDLE ||
      layout == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  try {
    auto data = std::make_unique<material_instance_data>(metadata);
    std::vector<resource_binding> resources;
    for (const auto& parameter : metadata.parameters()) {
      if (is_resource_type(parameter.type)) {
        resources.push_back({parameter.id, parameter.type, parameter.binding, GRANIT_NULL_HANDLE});
      }
    }

    granit_buffer buffer = GRANIT_NULL_HANDLE;
    if (metadata.constant_buffer_size() != 0) {
      granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
      desc.usage = GRANIT_BUFFER_USAGE_UNIFORM_BIT;
      desc.memory_location = GRANIT_MEMORY_LOCATION_UPLOAD;
      desc.size = metadata.constant_buffer_size();
      const auto result = granit_buffer_create(renderer, &desc, &buffer);
      if (result != GRANIT_SUCCESS) {
        return result;
      }
    }

    renderer_ = renderer;
    layout_ = layout;
    uniform_buffer_ = buffer;
    data_ = std::move(data);
    resources_ = std::move(resources);
    resources_dirty_ = true;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result material_gpu_instance::reset() noexcept {
  granit_result first_error = GRANIT_SUCCESS;
  if (bind_group_ != GRANIT_NULL_HANDLE) {
    first_error = granit_bind_group_destroy(renderer_, bind_group_);
  }
  if (uniform_buffer_ != GRANIT_NULL_HANDLE) {
    const auto result = granit_buffer_destroy(renderer_, uniform_buffer_);
    if (first_error == GRANIT_SUCCESS) {
      first_error = result;
    }
  }
  renderer_ = GRANIT_NULL_HANDLE;
  layout_ = GRANIT_NULL_HANDLE;
  uniform_buffer_ = GRANIT_NULL_HANDLE;
  bind_group_ = GRANIT_NULL_HANDLE;
  data_.reset();
  resources_.clear();
  resources_dirty_ = true;
  return first_error;
}

metadata_error material_gpu_instance::set(parameter_id id, parameter_type type,
                                          std::span<const std::byte> value) {
  if (data_ == nullptr) {
    return metadata_error::parameter_not_found;
  }
  return data_->set(id, type, value);
}

metadata_error material_gpu_instance::set_resource(parameter_id id, parameter_type type,
                                                   granit_handle resource) {
  if (!is_resource_type(type)) {
    return metadata_error::type_mismatch;
  }
  const auto found = std::ranges::find(resources_, id, &resource_binding::id);
  if (found == resources_.end()) {
    return metadata_error::parameter_not_found;
  }
  if (found->type != type) {
    return metadata_error::type_mismatch;
  }
  if (resource == GRANIT_NULL_HANDLE) {
    return metadata_error::invalid_resource;
  }
  if (found->resource != resource) {
    found->resource = resource;
    resources_dirty_ = true;
  }
  return metadata_error::none;
}

granit_result material_gpu_instance::flush() {
  if (renderer_ == GRANIT_NULL_HANDLE || data_ == nullptr) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto dirty = data_->dirty();
  if (!dirty.empty()) {
    const auto bytes = data_->bytes().subspan(dirty.offset, dirty.size);
    const auto result =
        granit_buffer_write(renderer_, uniform_buffer_, dirty.offset, bytes.data(), bytes.size());
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    data_->clear_dirty();
  }
  if (!resources_dirty_ && bind_group_ != GRANIT_NULL_HANDLE) {
    return GRANIT_SUCCESS;
  }
  if (std::ranges::any_of(resources_, [](const resource_binding& binding) {
        return binding.resource == GRANIT_NULL_HANDLE;
      })) {
    return GRANIT_ERROR_NOT_READY;
  }

  std::vector<granit_bind_group_entry> entries;
  try {
    entries.reserve(resources_.size() + (uniform_buffer_ == GRANIT_NULL_HANDLE ? 0U : 1U));
    if (uniform_buffer_ != GRANIT_NULL_HANDLE) {
      entries.push_back({.binding = 0,
                         .array_element = 0,
                         .resource = uniform_buffer_,
                         .offset = 0,
                         .size = data_->bytes().size()});
    }
    for (const auto& binding : resources_) {
      entries.push_back({.binding = binding.binding,
                         .array_element = 0,
                         .resource = binding.resource,
                         .offset = 0,
                         .size = GRANIT_WHOLE_SIZE});
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }

  granit_bind_group_desc desc = GRANIT_BIND_GROUP_DESC_INIT;
  desc.layout = layout_;
  desc.entry_count = static_cast<std::uint32_t>(entries.size());
  desc.entries = entries.data();
  granit_bind_group replacement = GRANIT_NULL_HANDLE;
  const auto result = granit_bind_group_create(renderer_, &desc, &replacement);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  const auto old = bind_group_;
  bind_group_ = replacement;
  resources_dirty_ = false;
  if (old != GRANIT_NULL_HANDLE) {
    return granit_bind_group_destroy(renderer_, old);
  }
  return GRANIT_SUCCESS;
}

} // namespace granit::material
