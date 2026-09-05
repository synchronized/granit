// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_MATERIAL_PACKAGE_H
#define GRANIT_MATERIAL_MATERIAL_PACKAGE_H

#include "material/material_metadata.h"

#include <granit/renderer/pipeline.h>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace granit::material {

inline constexpr std::uint32_t material_package_format_version = 4;

using material_pass_id = std::uint64_t;
using material_feature_id = std::uint64_t;
using material_variant_key = std::uint64_t;

enum class package_target : std::uint8_t {
  cross_backend,
};

enum class package_binding_model : std::uint8_t {
  bind_group,
  bindless,
};

using package_feature_flags = std::uint64_t;
inline constexpr package_feature_flags package_feature_bindless_resource_table = UINT64_C(1) << 0;

enum class package_shader_stage : std::uint8_t {
  vertex,
  fragment,
};

enum class package_error : std::uint8_t {
  none,
  unsupported_version,
  unsupported_target,
  unsupported_binding_model,
  unsupported_renderer_features,
  invalid_metadata,
  missing_variant,
  invalid_pass,
  invalid_feature,
  duplicate_feature,
  invalid_shader,
  duplicate_shader_stage,
  missing_shader_stage,
  duplicate_variant,
  variant_key_collision,
  invalid_pipeline_state,
};

struct material_vertex_attribute {
  std::uint32_t location = 0;
  granit_vertex_format format = GRANIT_VERTEX_FORMAT_FLOAT32;
  std::uint32_t offset = 0;

  friend bool operator==(const material_vertex_attribute&,
                         const material_vertex_attribute&) = default;
};

struct material_vertex_buffer_layout {
  std::uint32_t stride = 0;
  granit_vertex_step_mode step_mode = GRANIT_VERTEX_STEP_MODE_VERTEX;
  std::vector<material_vertex_attribute> attributes;

  friend bool operator==(const material_vertex_buffer_layout&,
                         const material_vertex_buffer_layout&) = default;
};

struct material_pipeline_state {
  std::vector<material_vertex_buffer_layout> vertex_buffers;
  granit_primitive_state primitive = GRANIT_PRIMITIVE_STATE_INIT;
  granit_depth_state depth = GRANIT_DEPTH_STATE_INIT;
  granit_color_blend_state color_blend = GRANIT_COLOR_BLEND_STATE_INIT;

  friend bool operator==(const material_pipeline_state& left,
                         const material_pipeline_state& right) noexcept;
};

struct material_feature_value {
  material_feature_id id = 0;
  std::uint32_t value = 0;

  friend bool operator==(const material_feature_value&, const material_feature_value&) = default;
};

struct material_shader_code {
  package_shader_stage stage = package_shader_stage::vertex;
  std::string entry_point;
  std::array<std::byte, 32> asset_id{};
  // 仅供内部测试与程序化模板使用；归档不会序列化这两项。
  std::vector<std::uint32_t> spirv;
  std::string wgsl;
};

struct material_variant_desc {
  material_pass_id pass = 0;
  std::vector<material_feature_value> features;
  std::vector<material_shader_code> shaders;
  material_pipeline_state pipeline;
};

struct material_package_desc {
  std::uint32_t format_version = material_package_format_version;
  package_target target = package_target::cross_backend;
  package_binding_model binding_model = package_binding_model::bind_group;
  package_feature_flags required_renderer_features = 0;
  metadata_desc metadata;
  std::vector<material_variant_desc> variants;
};

struct material_variant {
  material_pass_id pass = 0;
  material_variant_key key = 0;
  std::vector<material_feature_value> features;
  std::vector<material_shader_code> shaders;
  material_pipeline_state pipeline;
};

[[nodiscard]] material_feature_id make_feature_id(std::string_view name) noexcept;
[[nodiscard]] material_variant_key
make_variant_key(std::span<const material_feature_value> canonical_features) noexcept;

class material_package {
public:
  [[nodiscard]] static package_error build(material_package_desc desc, material_package& package);

  [[nodiscard]] std::uint32_t format_version() const noexcept { return format_version_; }
  [[nodiscard]] package_target target() const noexcept { return target_; }
  [[nodiscard]] package_binding_model binding_model() const noexcept { return binding_model_; }
  [[nodiscard]] package_feature_flags required_renderer_features() const noexcept {
    return required_renderer_features_;
  }
  [[nodiscard]] const material_metadata& metadata() const noexcept { return metadata_; }
  [[nodiscard]] std::span<const material_variant> variants() const noexcept { return variants_; }
  [[nodiscard]] const material_variant* find(material_pass_id pass,
                                             material_variant_key key) const noexcept;

private:
  std::uint32_t format_version_ = material_package_format_version;
  package_target target_ = package_target::cross_backend;
  package_binding_model binding_model_ = package_binding_model::bind_group;
  package_feature_flags required_renderer_features_ = 0;
  material_metadata metadata_;
  std::vector<material_variant> variants_;
};

} // namespace granit::material

#endif
