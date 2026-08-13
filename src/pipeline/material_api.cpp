// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.h>

#include "material/material_gpu_instance.h"
#include "material/material_package_archive.h"
#include "material/material_template_gpu.h"
#include "lighting/shadow_ibl_resources.h"
#include "pipeline/material_access.h"

#include <granit/renderer/pipeline.hpp>

#include <array>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

namespace {

constexpr uint64_t index_mask = UINT64_C(0xffffffff);
constexpr uint64_t generation_mask = UINT64_C(0x00ffffff);
constexpr uint64_t type_value = UINT64_C(0x41);

struct material_state {
  std::mutex mutex;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit::material::material_package package;
  granit::bind_group_layout object_layout;
  granit::bind_group_layout lighting_layout;
  granit::material::material_template_gpu material_template;
  granit::material::material_gpu_instance instance;
  bool alive = true;
};

struct material_slot {
  std::shared_ptr<material_state> state;
  uint32_t generation = 1;
};

std::mutex registry_mutex;
std::vector<material_slot> registry;

granit_handle encode(size_t index, uint32_t generation) {
  return (type_value << 56) | (static_cast<uint64_t>(generation) << 32) |
         (static_cast<uint64_t>(index) + 1);
}

bool decode(granit_handle handle, size_t& index, uint32_t& generation) {
  if ((handle >> 56) != type_value || (handle & index_mask) == 0)
    return false;
  index = static_cast<size_t>((handle & index_mask) - 1);
  generation = static_cast<uint32_t>((handle >> 32) & generation_mask);
  return generation != 0;
}

std::shared_ptr<material_state> find_material(granit_renderer renderer, granit_material material) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(material, index, generation))
    return {};
  std::scoped_lock lock{registry_mutex};
  if (index >= registry.size() || registry[index].generation != generation ||
      registry[index].state == nullptr || registry[index].state->renderer != renderer) {
    return {};
  }
  return registry[index].state;
}

bool convert_type(granit_material_parameter_type source,
                  granit::material::parameter_type& destination) {
  switch (source) {
  case GRANIT_MATERIAL_PARAMETER_BOOL32:
    destination = granit::material::parameter_type::bool32;
    return true;
  case GRANIT_MATERIAL_PARAMETER_INT32:
    destination = granit::material::parameter_type::int32;
    return true;
  case GRANIT_MATERIAL_PARAMETER_UINT32:
    destination = granit::material::parameter_type::uint32;
    return true;
  case GRANIT_MATERIAL_PARAMETER_FLOAT32:
    destination = granit::material::parameter_type::float32;
    return true;
  case GRANIT_MATERIAL_PARAMETER_FLOAT2:
    destination = granit::material::parameter_type::float2;
    return true;
  case GRANIT_MATERIAL_PARAMETER_FLOAT3:
    destination = granit::material::parameter_type::float3;
    return true;
  case GRANIT_MATERIAL_PARAMETER_FLOAT4:
    destination = granit::material::parameter_type::float4;
    return true;
  case GRANIT_MATERIAL_PARAMETER_MATRIX4:
    destination = granit::material::parameter_type::matrix4;
    return true;
  case GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW:
    destination = granit::material::parameter_type::texture_view;
    return true;
  case GRANIT_MATERIAL_PARAMETER_SAMPLER:
    destination = granit::material::parameter_type::sampler;
    return true;
  }
  return false;
}

granit_result map_metadata_error(granit::material::metadata_error error) {
  if (error == granit::material::metadata_error::none)
    return GRANIT_SUCCESS;
  if (error == granit::material::metadata_error::invalid_resource)
    return GRANIT_ERROR_INVALID_HANDLE;
  return GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result apply_updates(granit::material::material_gpu_instance& instance,
                            const granit_material_parameter_update* updates,
                            uint32_t update_count) {
  if (update_count != 0 && updates == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (uint32_t index = 0; index < update_count; ++index) {
    const auto& update = updates[index];
    granit::material::parameter_type type{};
    if (update.id == 0 || update.reserved != 0 || !convert_type(update.type, type))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    granit::material::metadata_error error{};
    if (granit::material::is_resource_type(type)) {
      if (update.data != nullptr || update.size != 0 || update.resource == GRANIT_NULL_HANDLE)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      error = instance.set_resource(update.id, type, update.resource);
    } else {
      if ((update.size != 0 && update.data == nullptr) || update.resource != GRANIT_NULL_HANDLE ||
          update.size > std::numeric_limits<size_t>::max()) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
      const auto bytes =
          std::span{static_cast<const std::byte*>(update.data), static_cast<size_t>(update.size)};
      error = instance.set(update.id, type, bytes);
    }
    const auto result = map_metadata_error(error);
    if (result != GRANIT_SUCCESS)
      return result;
  }
  return GRANIT_SUCCESS;
}

granit_result decode_archive(const granit_material_desc& desc,
                             granit::material::material_package& package) {
  if (desc.archive_data == nullptr || desc.archive_size == 0 ||
      desc.archive_size > std::numeric_limits<size_t>::max()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto bytes = std::span{static_cast<const std::byte*>(desc.archive_data),
                               static_cast<size_t>(desc.archive_size)};
  const auto error = granit::material::decode_material_package_archive(bytes, package);
  if (error == granit::material::archive_error::none)
    return GRANIT_SUCCESS;
  if (error == granit::material::archive_error::out_of_memory)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  if (error == granit::material::archive_error::unsupported_version ||
      error == granit::material::archive_error::unsupported_target ||
      error == granit::material::archive_error::unsupported_binding_model ||
      error == granit::material::archive_error::unsupported_renderer_features ||
      error == granit::material::archive_error::unsupported_compression) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return GRANIT_ERROR_INVALID_ARGUMENT;
}

} // namespace

granit_result
granit::pipeline::detail::validate_material_handle(granit_renderer renderer,
                                                   granit_material material) noexcept {
  return find_material(renderer, material) == nullptr ? GRANIT_ERROR_INVALID_HANDLE
                                                      : GRANIT_SUCCESS;
}

extern "C" uint64_t granit_material_parameter_id(const char* name, uint32_t name_length) {
  if (name == nullptr || name_length == 0)
    return 0;
  return granit::material::make_parameter_id({name, name_length});
}

extern "C" granit_result granit_material_create(granit_renderer renderer,
                                                const granit_material_desc* desc,
                                                granit_material* material) {
  if (material == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *material = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr ||
      desc->struct_size < sizeof(granit_material_desc) || desc->reserved != 0 ||
      desc->reserved_tail != 0 ||
      (desc->initial_update_count != 0 && desc->initial_updates == nullptr)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    auto state = std::make_shared<material_state>();
    state->renderer = renderer;
    auto result = decode_archive(*desc, state->package);
    if (result != GRANIT_SUCCESS)
      return result;
    const std::array object_entries{granit::bind_group_layout_entry{
        .binding = 0,
        .type = granit::binding_type::uniform_buffer,
        .array_count = 1,
        .visibility = granit::shader_stage_flags::vertex}};
    const auto object_result = state->object_layout.initialize(renderer, object_entries);
    if (granit::failed(object_result))
      return static_cast<granit_result>(object_result);
    const auto lighting_result = state->lighting_layout.initialize(
        renderer, granit::lighting::standard_lighting_layout_entries);
    if (granit::failed(lighting_result))
      return static_cast<granit_result>(lighting_result);
    const std::array additional_layouts{state->object_layout.native_handle(),
                                        state->lighting_layout.native_handle()};
    result = state->material_template.initialize(renderer, state->package, additional_layouts);
    if (result != GRANIT_SUCCESS)
      return result;
    result = state->instance.initialize(renderer, state->material_template.material_layout(),
                                        state->package.metadata());
    if (result != GRANIT_SUCCESS)
      return result;
    result = apply_updates(state->instance, desc->initial_updates, desc->initial_update_count);
    if (result != GRANIT_SUCCESS)
      return result;
    result = state->instance.flush();
    if (result != GRANIT_SUCCESS)
      return result;

    std::scoped_lock lock{registry_mutex};
    size_t index = 0;
    while (index < registry.size() && registry[index].state != nullptr)
      ++index;
    if (index == registry.size())
      registry.emplace_back();
    registry[index].state = std::move(state);
    *material = encode(index, registry[index].generation);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_material_update(granit_renderer renderer, granit_material material,
                                                const granit_material_parameter_update* updates,
                                                uint32_t update_count) {
  if (update_count != 0 && updates == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto state = find_material(renderer, material);
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    std::scoped_lock lock{state->mutex};
    if (!state->alive)
      return GRANIT_ERROR_INVALID_HANDLE;
    granit::material::material_gpu_instance replacement;
    granit::material::migration_report report;
    auto result = state->instance.prepare_migration(state->material_template.material_layout(),
                                                    state->package.metadata(), replacement, report);
    if (result != GRANIT_SUCCESS)
      return result;
    result = apply_updates(replacement, updates, update_count);
    if (result != GRANIT_SUCCESS)
      return result;
    result = replacement.flush();
    if (result != GRANIT_SUCCESS)
      return result;
    state->instance.swap(replacement);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_material_destroy(granit_renderer renderer,
                                                 granit_material material) {
  size_t index = 0;
  uint32_t generation = 0;
  if (!decode(material, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<material_state> removed;
  {
    std::scoped_lock lock{registry_mutex};
    if (index >= registry.size() || registry[index].generation != generation ||
        registry[index].state == nullptr || registry[index].state->renderer != renderer) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    removed = std::move(registry[index].state);
    registry[index].generation =
        registry[index].generation == generation_mask ? 1 : registry[index].generation + 1;
  }
  std::scoped_lock lock{removed->mutex};
  removed->alive = false;
  auto result = removed->instance.reset();
  const auto template_result = removed->material_template.reset();
  if (result == GRANIT_SUCCESS)
    result = template_result;
  const auto layout_result = static_cast<granit_result>(removed->object_layout.reset());
  if (result == GRANIT_SUCCESS)
    result = layout_result;
  const auto lighting_layout_result =
      static_cast<granit_result>(removed->lighting_layout.reset());
  if (result == GRANIT_SUCCESS)
    result = lighting_layout_result;
  return result;
}
