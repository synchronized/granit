// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_package.h"

#include <algorithm>
#include <array>
#include <utility>

namespace granit::material {
namespace {

constexpr std::uint64_t fnv_offset_basis = UINT64_C(14695981039346656037);
constexpr std::uint64_t fnv_prime = UINT64_C(1099511628211);

void hash_u32(std::uint64_t& hash, std::uint32_t value) noexcept {
  for (std::uint32_t shift = 0; shift < 32; shift += 8) {
    hash ^= static_cast<std::uint8_t>(value >> shift);
    hash *= fnv_prime;
  }
}

void hash_u64(std::uint64_t& hash, std::uint64_t value) noexcept {
  hash_u32(hash, static_cast<std::uint32_t>(value));
  hash_u32(hash, static_cast<std::uint32_t>(value >> 32U));
}

bool is_spirv(const std::vector<std::uint32_t>& code) noexcept {
  constexpr std::uint32_t spirv_magic = UINT32_C(0x07230203);
  return code.size() >= 5 && code.front() == spirv_magic;
}

std::uint32_t vertex_format_size(granit_vertex_format format) noexcept {
  switch (format) {
  case GRANIT_VERTEX_FORMAT_FLOAT32:
  case GRANIT_VERTEX_FORMAT_UINT32:
  case GRANIT_VERTEX_FORMAT_SINT32:
    return 4;
  case GRANIT_VERTEX_FORMAT_FLOAT32X2:
  case GRANIT_VERTEX_FORMAT_UINT32X2:
  case GRANIT_VERTEX_FORMAT_SINT32X2:
    return 8;
  case GRANIT_VERTEX_FORMAT_FLOAT32X3:
  case GRANIT_VERTEX_FORMAT_UINT32X3:
  case GRANIT_VERTEX_FORMAT_SINT32X3:
    return 12;
  case GRANIT_VERTEX_FORMAT_FLOAT32X4:
  case GRANIT_VERTEX_FORMAT_UINT32X4:
  case GRANIT_VERTEX_FORMAT_SINT32X4:
    return 16;
  default:
    return 0;
  }
}

bool pipeline_state_valid(const material_pipeline_state& state) noexcept {
  if (state.vertex_buffers.size() > 16 ||
      state.primitive.topology < GRANIT_PRIMITIVE_TOPOLOGY_POINT_LIST ||
      state.primitive.topology > GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
      state.primitive.front_face < GRANIT_FRONT_FACE_COUNTER_CLOCKWISE ||
      state.primitive.front_face > GRANIT_FRONT_FACE_CLOCKWISE ||
      state.primitive.cull_mode < GRANIT_CULL_MODE_NONE ||
      state.primitive.cull_mode > GRANIT_CULL_MODE_FRONT_AND_BACK ||
      state.primitive.polygon_mode < GRANIT_POLYGON_MODE_FILL ||
      state.primitive.polygon_mode > GRANIT_POLYGON_MODE_POINT || state.depth.test_enabled > 1 ||
      state.depth.write_enabled > 1 || state.depth.compare < GRANIT_COMPARE_OPERATION_NEVER ||
      state.depth.compare > GRANIT_COMPARE_OPERATION_ALWAYS || state.depth.reserved != 0 ||
      state.color_blend.enabled > 1 ||
      state.color_blend.source_color_factor < GRANIT_BLEND_FACTOR_ZERO ||
      state.color_blend.source_color_factor > GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA ||
      state.color_blend.destination_color_factor < GRANIT_BLEND_FACTOR_ZERO ||
      state.color_blend.destination_color_factor >
          GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA ||
      state.color_blend.source_alpha_factor < GRANIT_BLEND_FACTOR_ZERO ||
      state.color_blend.source_alpha_factor > GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA ||
      state.color_blend.destination_alpha_factor < GRANIT_BLEND_FACTOR_ZERO ||
      state.color_blend.destination_alpha_factor >
          GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA ||
      state.color_blend.color_operation < GRANIT_BLEND_OPERATION_ADD ||
      state.color_blend.color_operation > GRANIT_BLEND_OPERATION_MAX ||
      state.color_blend.alpha_operation < GRANIT_BLEND_OPERATION_ADD ||
      state.color_blend.alpha_operation > GRANIT_BLEND_OPERATION_MAX ||
      (state.color_blend.write_mask & ~GRANIT_COLOR_WRITE_ALL_BITS) != 0) {
    return false;
  }
  std::array<bool, 32> locations{};
  for (const auto& buffer : state.vertex_buffers) {
    if (buffer.stride == 0 || buffer.step_mode < GRANIT_VERTEX_STEP_MODE_VERTEX ||
        buffer.step_mode > GRANIT_VERTEX_STEP_MODE_INSTANCE || buffer.attributes.empty() ||
        buffer.attributes.size() > locations.size()) {
      return false;
    }
    for (const auto& attribute : buffer.attributes) {
      const auto size = vertex_format_size(attribute.format);
      if (attribute.location >= locations.size() || locations[attribute.location] || size == 0 ||
          attribute.offset >= buffer.stride || size > buffer.stride - attribute.offset) {
        return false;
      }
      locations[attribute.location] = true;
    }
  }
  return true;
}

} // namespace

bool operator==(const material_pipeline_state& left,
                const material_pipeline_state& right) noexcept {
  return left.vertex_buffers == right.vertex_buffers &&
         left.primitive.topology == right.primitive.topology &&
         left.primitive.front_face == right.primitive.front_face &&
         left.primitive.cull_mode == right.primitive.cull_mode &&
         left.primitive.polygon_mode == right.primitive.polygon_mode &&
         left.depth.test_enabled == right.depth.test_enabled &&
         left.depth.write_enabled == right.depth.write_enabled &&
         left.depth.compare == right.depth.compare &&
         left.color_blend.enabled == right.color_blend.enabled &&
         left.color_blend.source_color_factor == right.color_blend.source_color_factor &&
         left.color_blend.destination_color_factor == right.color_blend.destination_color_factor &&
         left.color_blend.color_operation == right.color_blend.color_operation &&
         left.color_blend.source_alpha_factor == right.color_blend.source_alpha_factor &&
         left.color_blend.destination_alpha_factor == right.color_blend.destination_alpha_factor &&
         left.color_blend.alpha_operation == right.color_blend.alpha_operation &&
         left.color_blend.write_mask == right.color_blend.write_mask;
}

material_feature_id make_feature_id(std::string_view name) noexcept {
  return make_parameter_id(name);
}

material_variant_key
make_variant_key(std::span<const material_feature_value> canonical_features) noexcept {
  auto hash = fnv_offset_basis;
  hash_u32(hash, static_cast<std::uint32_t>(canonical_features.size()));
  for (const auto& feature : canonical_features) {
    hash_u64(hash, feature.id);
    hash_u32(hash, feature.value);
  }
  return hash;
}

package_error material_package::build(material_package_desc desc, material_package& package) {
  if (desc.format_version != material_package_format_version) {
    return package_error::unsupported_version;
  }
  if (desc.target != package_target::vulkan_1_3) {
    return package_error::unsupported_target;
  }
  if (desc.binding_model != package_binding_model::bind_group) {
    return package_error::unsupported_binding_model;
  }
  if (desc.required_renderer_features != 0) {
    return package_error::unsupported_renderer_features;
  }

  material_metadata metadata;
  if (material_metadata::build(std::move(desc.metadata), metadata) != metadata_error::none) {
    return package_error::invalid_metadata;
  }
  if (desc.variants.empty()) {
    return package_error::missing_variant;
  }

  std::vector<material_variant> variants;
  variants.reserve(desc.variants.size());
  for (auto& source : desc.variants) {
    if (source.pass == 0) {
      return package_error::invalid_pass;
    }
    std::ranges::sort(source.features, {}, &material_feature_value::id);
    for (std::size_t index = 0; index < source.features.size(); ++index) {
      if (source.features[index].id == 0) {
        return package_error::invalid_feature;
      }
      if (index > 0 && source.features[index - 1].id == source.features[index].id) {
        return package_error::duplicate_feature;
      }
    }

    std::array<bool, 2> stages{};
    for (const auto& shader : source.shaders) {
      const auto stage_index = static_cast<std::size_t>(shader.stage);
      if (stage_index >= stages.size() || shader.entry_point.empty() || !is_spirv(shader.spirv)) {
        return package_error::invalid_shader;
      }
      if (stages[stage_index]) {
        return package_error::duplicate_shader_stage;
      }
      stages[stage_index] = true;
    }
    if (!stages[static_cast<std::size_t>(package_shader_stage::vertex)] ||
        !stages[static_cast<std::size_t>(package_shader_stage::fragment)]) {
      return package_error::missing_shader_stage;
    }
    if (!pipeline_state_valid(source.pipeline)) {
      return package_error::invalid_pipeline_state;
    }

    material_variant variant{.pass = source.pass,
                             .key = make_variant_key(source.features),
                             .features = std::move(source.features),
                             .shaders = std::move(source.shaders),
                             .pipeline = std::move(source.pipeline)};
    const auto same_key = std::ranges::find_if(variants, [&](const auto& existing) {
      return existing.pass == variant.pass && existing.key == variant.key;
    });
    if (same_key != variants.end()) {
      return same_key->features == variant.features ? package_error::duplicate_variant
                                                    : package_error::variant_key_collision;
    }
    variants.push_back(std::move(variant));
  }

  std::ranges::sort(variants, [](const auto& left, const auto& right) {
    return left.pass < right.pass || (left.pass == right.pass && left.key < right.key);
  });
  material_package built;
  built.format_version_ = desc.format_version;
  built.target_ = desc.target;
  built.binding_model_ = desc.binding_model;
  built.required_renderer_features_ = desc.required_renderer_features;
  built.metadata_ = std::move(metadata);
  built.variants_ = std::move(variants);
  package = std::move(built);
  return package_error::none;
}

const material_variant* material_package::find(material_pass_id pass,
                                               material_variant_key key) const noexcept {
  const auto found =
      std::ranges::lower_bound(variants_, std::pair{pass, key}, {}, [](const auto& variant) {
        return std::pair{variant.pass, variant.key};
      });
  return found != variants_.end() && found->pass == pass && found->key == key ? &*found : nullptr;
}

} // namespace granit::material
