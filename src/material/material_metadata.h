// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_MATERIAL_METADATA_H
#define GRANIT_MATERIAL_MATERIAL_METADATA_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace granit::material {

using parameter_id = std::uint64_t;

enum class parameter_type : std::uint8_t {
  bool32,
  int32,
  uint32,
  float32,
  float2,
  float3,
  float4,
  matrix4,
  texture_view,
  sampler,
};

enum class metadata_error : std::uint8_t {
  none,
  invalid_name,
  invalid_id,
  duplicate_name,
  id_collision,
  invalid_type,
  invalid_array,
  invalid_layout,
  overlapping_parameters,
  invalid_default_value,
  invalid_resource,
  parameter_not_found,
  type_mismatch,
  size_mismatch,
};

struct parameter_desc {
  std::string name;
  parameter_id id = 0;
  parameter_type type = parameter_type::float32;
  std::uint32_t offset = 0;
  std::uint32_t array_count = 1;
  std::uint32_t array_stride = 0;
  std::uint32_t binding = 0;
  std::vector<std::byte> default_value;
};

struct metadata_desc {
  std::uint32_t constant_buffer_size = 0;
  std::vector<parameter_desc> parameters;
};

struct dirty_range {
  std::uint32_t offset = 0;
  std::uint32_t size = 0;

  [[nodiscard]] bool empty() const noexcept { return size == 0; }
};

[[nodiscard]] parameter_id make_parameter_id(std::string_view name) noexcept;
[[nodiscard]] bool is_resource_type(parameter_type type) noexcept;
[[nodiscard]] std::uint32_t parameter_element_size(parameter_type type) noexcept;

class material_metadata {
public:
  [[nodiscard]] static metadata_error build(metadata_desc desc, material_metadata& metadata);

  [[nodiscard]] std::uint32_t constant_buffer_size() const noexcept {
    return constant_buffer_size_;
  }
  [[nodiscard]] std::span<const parameter_desc> parameters() const noexcept { return parameters_; }
  [[nodiscard]] const parameter_desc* find(parameter_id id) const noexcept;

private:
  std::uint32_t constant_buffer_size_ = 0;
  std::vector<parameter_desc> parameters_;
};

class material_instance_data {
public:
  explicit material_instance_data(const material_metadata& metadata);

  [[nodiscard]] metadata_error set(parameter_id id, parameter_type type,
                                   std::span<const std::byte> value);
  [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return bytes_; }
  [[nodiscard]] const material_metadata& metadata() const noexcept { return *metadata_; }
  [[nodiscard]] dirty_range dirty() const noexcept { return dirty_; }
  void clear_dirty() noexcept { dirty_ = {}; }

private:
  const material_metadata* metadata_ = nullptr;
  std::vector<std::byte> bytes_;
  dirty_range dirty_;
};

} // namespace granit::material

#endif
