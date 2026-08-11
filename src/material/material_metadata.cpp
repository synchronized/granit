// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_metadata.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>

namespace granit::material {
namespace {

constexpr std::uint32_t max_constant_buffer_size = 1024U * 1024U;

std::uint64_t storage_size(const parameter_desc& parameter) noexcept {
  const auto element_size = parameter_element_size(parameter.type);
  if (element_size == 0 || parameter.array_count == 0) {
    return 0;
  }
  if (parameter.array_count == 1) {
    return element_size;
  }
  return static_cast<std::uint64_t>(parameter.array_stride) * (parameter.array_count - 1U) +
         element_size;
}

} // namespace

parameter_id make_parameter_id(std::string_view name) noexcept {
  constexpr parameter_id offset_basis = UINT64_C(14695981039346656037);
  constexpr parameter_id prime = UINT64_C(1099511628211);
  auto hash = offset_basis;
  for (const auto character : name) {
    hash ^= static_cast<std::uint8_t>(character);
    hash *= prime;
  }
  return hash;
}

bool is_resource_type(parameter_type type) noexcept {
  return type == parameter_type::texture_view || type == parameter_type::sampler;
}

std::uint32_t parameter_element_size(parameter_type type) noexcept {
  switch (type) {
  case parameter_type::bool32:
  case parameter_type::int32:
  case parameter_type::uint32:
  case parameter_type::float32:
    return 4;
  case parameter_type::float2:
    return 8;
  case parameter_type::float3:
    return 12;
  case parameter_type::float4:
    return 16;
  case parameter_type::matrix4:
    return 64;
  case parameter_type::texture_view:
  case parameter_type::sampler:
    return 0;
  }
  return 0;
}

metadata_error material_metadata::build(metadata_desc desc, material_metadata& metadata) {
  if (desc.constant_buffer_size > max_constant_buffer_size || desc.constant_buffer_size % 4U != 0) {
    return metadata_error::invalid_layout;
  }

  std::unordered_map<std::string, parameter_id> names;
  std::unordered_map<parameter_id, std::string> ids;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> occupied_ranges;
  occupied_ranges.reserve(desc.parameters.size());

  for (auto& parameter : desc.parameters) {
    if (parameter.name.empty()) {
      return metadata_error::invalid_name;
    }
    const auto expected_id = make_parameter_id(parameter.name);
    if (parameter.id == 0) {
      parameter.id = expected_id;
    } else if (parameter.id != expected_id) {
      return metadata_error::invalid_id;
    }
    if (!names.emplace(parameter.name, parameter.id).second) {
      return metadata_error::duplicate_name;
    }
    const auto [id_entry, inserted] = ids.emplace(parameter.id, parameter.name);
    if (!inserted && id_entry->second != parameter.name) {
      return metadata_error::id_collision;
    }

    if (is_resource_type(parameter.type)) {
      if (parameter.array_count != 1 || parameter.offset != 0 || parameter.array_stride != 0 ||
          !parameter.default_value.empty()) {
        return metadata_error::invalid_layout;
      }
      continue;
    }

    const auto element_size = parameter_element_size(parameter.type);
    if (element_size == 0) {
      return metadata_error::invalid_type;
    }
    if (parameter.array_count == 0 ||
        (parameter.array_count > 1 &&
         (parameter.array_stride < element_size || parameter.array_stride % 4U != 0))) {
      return metadata_error::invalid_array;
    }
    const auto size = storage_size(parameter);
    const auto end = static_cast<std::uint64_t>(parameter.offset) + size;
    if (parameter.offset % 4U != 0 || end > desc.constant_buffer_size ||
        end > std::numeric_limits<std::uint32_t>::max()) {
      return metadata_error::invalid_layout;
    }
    if (!parameter.default_value.empty() && parameter.default_value.size() != size) {
      return metadata_error::invalid_default_value;
    }
    for (const auto& [occupied_begin, occupied_end] : occupied_ranges) {
      if (parameter.offset < occupied_end && end > occupied_begin) {
        return metadata_error::overlapping_parameters;
      }
    }
    occupied_ranges.emplace_back(parameter.offset, end);
  }

  material_metadata built;
  built.constant_buffer_size_ = desc.constant_buffer_size;
  built.parameters_ = std::move(desc.parameters);
  metadata = std::move(built);
  return metadata_error::none;
}

const parameter_desc* material_metadata::find(parameter_id id) const noexcept {
  const auto found = std::ranges::find(parameters_, id, &parameter_desc::id);
  return found == parameters_.end() ? nullptr : &*found;
}

material_instance_data::material_instance_data(const material_metadata& metadata)
    : metadata_(&metadata), bytes_(metadata.constant_buffer_size()) {
  for (const auto& parameter : metadata.parameters()) {
    if (!parameter.default_value.empty()) {
      std::memcpy(bytes_.data() + parameter.offset, parameter.default_value.data(),
                  parameter.default_value.size());
    }
  }
  if (!bytes_.empty()) {
    dirty_ = {.offset = 0, .size = static_cast<std::uint32_t>(bytes_.size())};
  }
}

metadata_error material_instance_data::set(parameter_id id, parameter_type type,
                                           std::span<const std::byte> value) {
  const auto* parameter = metadata_->find(id);
  if (parameter == nullptr) {
    return metadata_error::parameter_not_found;
  }
  if (parameter->type != type || is_resource_type(type)) {
    return metadata_error::type_mismatch;
  }
  const auto expected_size = storage_size(*parameter);
  if (value.size() != expected_size) {
    return metadata_error::size_mismatch;
  }
  auto destination = std::span{bytes_}.subspan(parameter->offset, value.size());
  if (std::ranges::equal(destination, value)) {
    return metadata_error::none;
  }
  std::ranges::copy(value, destination.begin());

  const auto begin = parameter->offset;
  const auto end = static_cast<std::uint32_t>(begin + value.size());
  if (dirty_.empty()) {
    dirty_ = {.offset = begin, .size = end - begin};
  } else {
    const auto dirty_end = dirty_.offset + dirty_.size;
    const auto merged_begin = std::min(dirty_.offset, begin);
    const auto merged_end = std::max(dirty_end, end);
    dirty_ = {.offset = merged_begin, .size = merged_end - merged_begin};
  }
  return metadata_error::none;
}

} // namespace granit::material
