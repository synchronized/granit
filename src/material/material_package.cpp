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

} // namespace

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

    material_variant variant{.pass = source.pass,
                             .key = make_variant_key(source.features),
                             .features = std::move(source.features),
                             .shaders = std::move(source.shaders)};
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
