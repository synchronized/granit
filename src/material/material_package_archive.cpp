// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_package_archive.h"

#include <algorithm>
#include <map>
#include <new>
#include <set>
#include <string>

namespace granit::material {
namespace {

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
  for (std::uint32_t index = 0; index < 4; ++index) {
    bytes.push_back(static_cast<std::byte>(value >> (index * 8U)));
  }
}

void append_u64(std::vector<std::byte>& bytes, std::uint64_t value) {
  append_u32(bytes, static_cast<std::uint32_t>(value));
  append_u32(bytes, static_cast<std::uint32_t>(value >> 32U));
}

void append_bytes(std::vector<std::byte>& destination, std::span<const std::byte> source) {
  destination.insert(destination.end(), source.begin(), source.end());
}

struct string_reference {
  std::uint32_t offset = 0;
  std::uint32_t length = 0;
};

} // namespace

archive_error encode_material_package_archive(const material_package& package,
                                              std::vector<std::byte>& bytes) noexcept {
  try {
    std::set<std::string> strings;
    for (const auto& parameter : package.metadata().parameters()) {
      strings.insert(parameter.name);
    }
    for (const auto& variant : package.variants()) {
      for (const auto& shader : variant.shaders) {
        strings.insert(shader.entry_point);
      }
    }

    std::vector<std::byte> string_table;
    std::map<std::string, string_reference> string_references;
    for (const auto& value : strings) {
      const auto offset = static_cast<std::uint32_t>(string_table.size());
      string_table.insert(string_table.end(), reinterpret_cast<const std::byte*>(value.data()),
                          reinterpret_cast<const std::byte*>(value.data() + value.size()));
      string_references.emplace(value,
                                string_reference{offset, static_cast<std::uint32_t>(value.size())});
    }
    if (string_table.empty()) {
      string_table.push_back(std::byte{0});
    }

    std::vector<const parameter_desc*> parameters;
    parameters.reserve(package.metadata().parameters().size());
    for (const auto& parameter : package.metadata().parameters()) {
      parameters.push_back(&parameter);
    }
    std::ranges::sort(parameters, {}, &parameter_desc::id);
    constexpr std::uint32_t metadata_header_size = 16;
    constexpr std::uint32_t parameter_record_size = 48;
    std::vector<std::byte> metadata;
    metadata.reserve(metadata_header_size + parameters.size() * parameter_record_size);
    append_u32(metadata, package.metadata().constant_buffer_size());
    append_u32(metadata, static_cast<std::uint32_t>(parameters.size()));
    append_u32(metadata, parameter_record_size);
    append_u32(metadata, 0);
    std::vector<std::byte> default_values;
    for (const auto* parameter : parameters) {
      const auto name = string_references.at(parameter->name);
      append_u64(metadata, parameter->id);
      append_u32(metadata, name.offset);
      append_u32(metadata, name.length);
      append_u32(metadata, static_cast<std::uint32_t>(parameter->type));
      append_u32(metadata, parameter->offset);
      append_u32(metadata, parameter->array_count);
      append_u32(metadata, parameter->array_stride);
      append_u32(metadata, parameter->binding);
      append_u32(metadata, static_cast<std::uint32_t>(default_values.size()));
      append_u32(metadata, static_cast<std::uint32_t>(parameter->default_value.size()));
      append_u32(metadata, 0);
      append_bytes(default_values, parameter->default_value);
    }
    append_bytes(metadata, default_values);

    std::set<material_feature_id> feature_ids;
    std::set<material_pass_id> pass_ids;
    for (const auto& variant : package.variants()) {
      pass_ids.insert(variant.pass);
      for (const auto& feature : variant.features) {
        feature_ids.insert(feature.id);
      }
    }
    std::vector<std::byte> feature_definitions;
    append_u32(feature_definitions, static_cast<std::uint32_t>(feature_ids.size()));
    append_u32(feature_definitions, 16);
    for (const auto id : feature_ids) {
      append_u64(feature_definitions, id);
      append_u32(feature_definitions, 0);
      append_u32(feature_definitions, 0);
    }

    std::vector<std::byte> pass_definitions;
    append_u32(pass_definitions, static_cast<std::uint32_t>(pass_ids.size()));
    append_u32(pass_definitions, 16);
    std::uint32_t variant_cursor = 0;
    for (const auto pass : pass_ids) {
      const auto count = static_cast<std::uint32_t>(
          std::ranges::count(package.variants(), pass, &material_variant::pass));
      append_u64(pass_definitions, pass);
      append_u32(pass_definitions, variant_cursor);
      append_u32(pass_definitions, count);
      variant_cursor += count;
    }

    std::vector<std::byte> variant_records;
    std::vector<std::byte> feature_values;
    append_u32(variant_records, static_cast<std::uint32_t>(package.variants().size()));
    append_u32(variant_records, 40);
    append_u32(variant_records, 16);
    append_u32(variant_records, 0);
    std::vector<std::byte> shader_records;
    std::vector<std::byte> spirv_data;
    append_u32(shader_records, 0);
    append_u32(shader_records, 32);
    std::uint32_t feature_cursor = 0;
    std::uint32_t shader_cursor = 0;
    for (const auto& variant : package.variants()) {
      std::vector<const material_shader_code*> shaders;
      shaders.reserve(variant.shaders.size());
      for (const auto& shader : variant.shaders) {
        shaders.push_back(&shader);
      }
      std::ranges::sort(shaders, {}, &material_shader_code::stage);

      append_u64(variant_records, variant.pass);
      append_u64(variant_records, variant.key);
      append_u32(variant_records, feature_cursor);
      append_u32(variant_records, static_cast<std::uint32_t>(variant.features.size()));
      append_u32(variant_records, shader_cursor);
      append_u32(variant_records, static_cast<std::uint32_t>(shaders.size()));
      append_u64(variant_records, 0);
      for (const auto& feature : variant.features) {
        append_u64(feature_values, feature.id);
        append_u32(feature_values, feature.value);
        append_u32(feature_values, 0);
      }
      feature_cursor += static_cast<std::uint32_t>(variant.features.size());

      for (const auto* shader : shaders) {
        const auto entry = string_references.at(shader->entry_point);
        append_u32(shader_records, static_cast<std::uint32_t>(shader->stage));
        append_u32(shader_records, entry.offset);
        append_u32(shader_records, entry.length);
        append_u32(shader_records, 0);
        append_u64(shader_records, spirv_data.size());
        append_u64(shader_records, shader->spirv.size() * sizeof(std::uint32_t));
        for (const auto word : shader->spirv) {
          append_u32(spirv_data, word);
        }
      }
      shader_cursor += static_cast<std::uint32_t>(shaders.size());
    }
    append_bytes(variant_records, feature_values);
    const auto shader_count = shader_cursor;
    for (std::uint32_t index = 0; index < 4; ++index) {
      shader_records[index] = static_cast<std::byte>(shader_count >> (index * 8U));
    }

    const std::array sections{
        material_archive_section_source{archive_section_type::string_table,
                                        archive_section_required, 1, string_table},
        material_archive_section_source{archive_section_type::material_metadata,
                                        archive_section_required, 8, metadata},
        material_archive_section_source{archive_section_type::feature_definitions,
                                        archive_section_required, 8, feature_definitions},
        material_archive_section_source{archive_section_type::pass_definitions,
                                        archive_section_required, 8, pass_definitions},
        material_archive_section_source{archive_section_type::variant_records,
                                        archive_section_required, 8, variant_records},
        material_archive_section_source{archive_section_type::shader_records,
                                        archive_section_required, 8, shader_records},
        material_archive_section_source{archive_section_type::spirv_data, archive_section_required,
                                        4, spirv_data}};
    return encode_material_archive({.target_environment = material_archive_target_vulkan_1_3,
                                    .binding_model = material_archive_binding_model_bind_group,
                                    .required_renderer_features = 0,
                                    .sections = sections},
                                   bytes);
  } catch (const std::bad_alloc&) {
    return archive_error::out_of_memory;
  } catch (...) {
    return archive_error::invalid_section;
  }
}

} // namespace granit::material
