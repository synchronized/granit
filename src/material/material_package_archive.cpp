// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_package_archive.h"

#include <algorithm>
#include <array>
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

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  return std::to_integer<std::uint32_t>(bytes[offset]) |
         (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 8U) |
         (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 16U) |
         (std::to_integer<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::uint64_t read_u64(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  return read_u32(bytes, offset) | (static_cast<std::uint64_t>(read_u32(bytes, offset + 4)) << 32U);
}

bool record_range_valid(std::uint32_t count, std::uint32_t record_size, std::size_t header_size,
                        std::size_t section_size) noexcept {
  return record_size != 0 &&
         count <= (section_size - std::min(header_size, section_size)) / record_size &&
         header_size <= section_size;
}

std::span<const std::byte> find_section(std::span<const std::byte> bytes,
                                        const material_archive_layout& layout,
                                        archive_section_type type) noexcept {
  const auto found = std::ranges::find(layout.sections, static_cast<std::uint32_t>(type),
                                       &material_archive_section::type);
  return found == layout.sections.end()
             ? std::span<const std::byte>{}
             : bytes.subspan(static_cast<std::size_t>(found->offset),
                             static_cast<std::size_t>(found->stored_size));
}

bool utf8_valid(std::span<const std::byte> bytes) noexcept {
  std::size_t index = 0;
  while (index < bytes.size()) {
    const auto first = std::to_integer<std::uint8_t>(bytes[index]);
    if (first <= 0x7fU) {
      ++index;
      continue;
    }
    std::size_t continuation_count = 0;
    std::uint32_t code_point = 0;
    std::uint32_t minimum = 0;
    if (first >= 0xc2U && first <= 0xdfU) {
      continuation_count = 1;
      code_point = first & 0x1fU;
      minimum = 0x80U;
    } else if (first >= 0xe0U && first <= 0xefU) {
      continuation_count = 2;
      code_point = first & 0x0fU;
      minimum = 0x800U;
    } else if (first >= 0xf0U && first <= 0xf4U) {
      continuation_count = 3;
      code_point = first & 0x07U;
      minimum = 0x10000U;
    } else {
      return false;
    }
    if (continuation_count > bytes.size() - index - 1) {
      return false;
    }
    for (std::size_t continuation = 0; continuation < continuation_count; ++continuation) {
      const auto value = std::to_integer<std::uint8_t>(bytes[index + continuation + 1]);
      if ((value & 0xc0U) != 0x80U) {
        return false;
      }
      code_point = (code_point << 6U) | (value & 0x3fU);
    }
    if (code_point < minimum || code_point > 0x10ffffU ||
        (code_point >= 0xd800U && code_point <= 0xdfffU)) {
      return false;
    }
    index += continuation_count + 1;
  }
  return true;
}

bool string_valid(std::span<const std::byte> table, std::uint32_t offset,
                  std::uint32_t length) noexcept {
  return length != 0 && offset <= table.size() && length <= table.size() - offset &&
         utf8_valid(table.subspan(offset, length));
}

std::string read_string(std::span<const std::byte> table, std::uint32_t offset,
                        std::uint32_t length) {
  const auto* first = reinterpret_cast<const char*>(table.data() + offset);
  return {first, first + length};
}

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
    append_u32(shader_records, 0);
    append_u32(shader_records, 48);
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
        append_bytes(shader_records, shader->asset_id);
      }
      shader_cursor += static_cast<std::uint32_t>(shaders.size());
    }
    append_bytes(variant_records, feature_values);
    const auto shader_count = shader_cursor;
    for (std::uint32_t index = 0; index < 4; ++index) {
      shader_records[index] = static_cast<std::byte>(shader_count >> (index * 8U));
    }

    constexpr std::uint32_t pipeline_header_size = 24;
    constexpr std::uint32_t pipeline_state_record_size = 80;
    constexpr std::uint32_t vertex_buffer_record_size = 16;
    constexpr std::uint32_t vertex_attribute_record_size = 16;
    std::vector<std::byte> pipeline_state_records;
    std::vector<std::byte> vertex_buffer_records;
    std::vector<std::byte> vertex_attribute_records;
    std::uint32_t buffer_cursor = 0;
    std::uint32_t attribute_cursor = 0;
    for (const auto& variant : package.variants()) {
      const auto& state = variant.pipeline;
      append_u32(pipeline_state_records, buffer_cursor);
      append_u32(pipeline_state_records, static_cast<std::uint32_t>(state.vertex_buffers.size()));
      append_u32(pipeline_state_records, state.primitive.topology);
      append_u32(pipeline_state_records, state.primitive.front_face);
      append_u32(pipeline_state_records, state.primitive.cull_mode);
      append_u32(pipeline_state_records, state.primitive.polygon_mode);
      append_u32(pipeline_state_records, state.depth.test_enabled);
      append_u32(pipeline_state_records, state.depth.write_enabled);
      append_u32(pipeline_state_records, state.depth.compare);
      append_u32(pipeline_state_records, 0);
      append_u32(pipeline_state_records, state.color_blend.enabled);
      append_u32(pipeline_state_records, state.color_blend.source_color_factor);
      append_u32(pipeline_state_records, state.color_blend.destination_color_factor);
      append_u32(pipeline_state_records, state.color_blend.color_operation);
      append_u32(pipeline_state_records, state.color_blend.source_alpha_factor);
      append_u32(pipeline_state_records, state.color_blend.destination_alpha_factor);
      append_u32(pipeline_state_records, state.color_blend.alpha_operation);
      append_u32(pipeline_state_records, state.color_blend.write_mask);
      append_u32(pipeline_state_records, 0);
      append_u32(pipeline_state_records, 0);
      for (const auto& buffer : state.vertex_buffers) {
        append_u32(vertex_buffer_records, buffer.stride);
        append_u32(vertex_buffer_records, buffer.step_mode);
        append_u32(vertex_buffer_records, attribute_cursor);
        append_u32(vertex_buffer_records, static_cast<std::uint32_t>(buffer.attributes.size()));
        for (const auto& attribute : buffer.attributes) {
          append_u32(vertex_attribute_records, attribute.location);
          append_u32(vertex_attribute_records, attribute.format);
          append_u32(vertex_attribute_records, attribute.offset);
          append_u32(vertex_attribute_records, 0);
        }
        attribute_cursor += static_cast<std::uint32_t>(buffer.attributes.size());
      }
      buffer_cursor += static_cast<std::uint32_t>(state.vertex_buffers.size());
    }
    std::vector<std::byte> pipeline_states;
    pipeline_states.reserve(pipeline_header_size + pipeline_state_records.size() +
                            vertex_buffer_records.size() + vertex_attribute_records.size());
    append_u32(pipeline_states, static_cast<std::uint32_t>(package.variants().size()));
    append_u32(pipeline_states, pipeline_state_record_size);
    append_u32(pipeline_states, buffer_cursor);
    append_u32(pipeline_states, vertex_buffer_record_size);
    append_u32(pipeline_states, attribute_cursor);
    append_u32(pipeline_states, vertex_attribute_record_size);
    append_bytes(pipeline_states, pipeline_state_records);
    append_bytes(pipeline_states, vertex_buffer_records);
    append_bytes(pipeline_states, vertex_attribute_records);

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
        material_archive_section_source{archive_section_type::pipeline_states,
                                        archive_section_required, 8, pipeline_states}};
    return encode_material_archive({.target_environment = material_archive_target_cross_backend,
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

archive_error decode_material_package_archive(std::span<const std::byte> bytes,
                                              material_package& package) noexcept {
  material_archive_layout layout;
  const auto archive_result = parse_material_archive_layout(bytes, layout);
  if (archive_result != archive_error::none) {
    return archive_result;
  }
  try {
    constexpr std::uint32_t max_parameters = 4096;
    constexpr std::uint32_t max_features = 4096;
    constexpr std::uint32_t max_passes = 256;
    constexpr std::uint32_t max_variants = 65536;
    constexpr std::uint32_t max_shaders = 131072;
    const auto strings = find_section(bytes, layout, archive_section_type::string_table);
    const auto metadata = find_section(bytes, layout, archive_section_type::material_metadata);
    const auto feature_definitions =
        find_section(bytes, layout, archive_section_type::feature_definitions);
    const auto pass_definitions =
        find_section(bytes, layout, archive_section_type::pass_definitions);
    const auto variant_records = find_section(bytes, layout, archive_section_type::variant_records);
    const auto shader_records = find_section(bytes, layout, archive_section_type::shader_records);
    const auto pipeline_states = find_section(bytes, layout, archive_section_type::pipeline_states);

    if (!utf8_valid(strings) || metadata.size() < 16 ||
        feature_definitions.size() < 8 ||
        pass_definitions.size() < 8 || variant_records.size() < 16 || shader_records.size() < 8 ||
        pipeline_states.size() < 24) {
      return archive_error::invalid_semantic_data;
    }

    material_package_desc desc;
    desc.format_version = material_package_format_version;
    desc.target = package_target::cross_backend;
    desc.binding_model = package_binding_model::bind_group;
    desc.required_renderer_features = 0;

    desc.metadata.constant_buffer_size = read_u32(metadata, 0);
    const auto parameter_count = read_u32(metadata, 4);
    const auto parameter_record_size = read_u32(metadata, 8);
    if (parameter_count > max_parameters || parameter_record_size != 48 ||
        !record_range_valid(parameter_count, parameter_record_size, 16, metadata.size())) {
      return archive_error::invalid_semantic_data;
    }
    const auto default_values_offset = 16U + parameter_count * parameter_record_size;
    desc.metadata.parameters.reserve(parameter_count);
    for (std::uint32_t index = 0; index < parameter_count; ++index) {
      const auto record = 16U + index * parameter_record_size;
      const auto name_offset = read_u32(metadata, record + 8);
      const auto name_length = read_u32(metadata, record + 12);
      const auto default_offset = read_u32(metadata, record + 36);
      const auto default_size = read_u32(metadata, record + 40);
      if (!string_valid(strings, name_offset, name_length) ||
          default_offset > metadata.size() - default_values_offset ||
          default_size > metadata.size() - default_values_offset - default_offset ||
          read_u32(metadata, record + 44) != 0) {
        return archive_error::invalid_semantic_data;
      }
      parameter_desc parameter{.name = read_string(strings, name_offset, name_length),
                               .id = read_u64(metadata, record),
                               .type = static_cast<parameter_type>(read_u32(metadata, record + 16)),
                               .offset = read_u32(metadata, record + 20),
                               .array_count = read_u32(metadata, record + 24),
                               .array_stride = read_u32(metadata, record + 28),
                               .binding = read_u32(metadata, record + 32),
                               .default_value = {}};
      const auto defaults = metadata.subspan(default_values_offset + default_offset, default_size);
      parameter.default_value.assign(defaults.begin(), defaults.end());
      desc.metadata.parameters.push_back(std::move(parameter));
    }

    const auto feature_count = read_u32(feature_definitions, 0);
    const auto feature_record_size = read_u32(feature_definitions, 4);
    if (feature_count > max_features || feature_record_size != 16 ||
        !record_range_valid(feature_count, feature_record_size, 8, feature_definitions.size())) {
      return archive_error::invalid_semantic_data;
    }
    std::set<material_feature_id> feature_ids;
    for (std::uint32_t index = 0; index < feature_count; ++index) {
      const auto record = 8U + index * feature_record_size;
      const auto id = read_u64(feature_definitions, record);
      if (id == 0 || read_u32(feature_definitions, record + 12) != 0 ||
          !feature_ids.insert(id).second) {
        return archive_error::invalid_semantic_data;
      }
    }

    const auto shader_count = read_u32(shader_records, 0);
    const auto shader_record_size = read_u32(shader_records, 4);
    if (shader_count > max_shaders || shader_record_size != 48 ||
        !record_range_valid(shader_count, shader_record_size, 8, shader_records.size())) {
      return archive_error::invalid_semantic_data;
    }
    std::vector<material_shader_code> shaders;
    shaders.reserve(shader_count);
    for (std::uint32_t index = 0; index < shader_count; ++index) {
      const auto record = 8U + index * shader_record_size;
      const auto stage = read_u32(shader_records, record);
      const auto name_offset = read_u32(shader_records, record + 4);
      const auto name_length = read_u32(shader_records, record + 8);
      if (stage > static_cast<std::uint32_t>(package_shader_stage::fragment) ||
          !string_valid(strings, name_offset, name_length) ||
          read_u32(shader_records, record + 12) != 0) {
        return archive_error::invalid_semantic_data;
      }
      material_shader_code shader{.stage = static_cast<package_shader_stage>(stage),
                                  .entry_point = read_string(strings, name_offset, name_length),
                                  .asset_id = {},
                                  .spirv = {},
                                  .wgsl = {}};
      std::ranges::copy(shader_records.subspan(record + 16, shader.asset_id.size()),
                        shader.asset_id.begin());
      shaders.push_back(std::move(shader));
    }

    const auto variant_count = read_u32(variant_records, 0);
    const auto variant_record_size = read_u32(variant_records, 4);
    const auto feature_value_size = read_u32(variant_records, 8);
    if (variant_count == 0 || variant_count > max_variants || variant_record_size != 40 ||
        feature_value_size != 16 || read_u32(variant_records, 12) != 0 ||
        !record_range_valid(variant_count, variant_record_size, 16, variant_records.size())) {
      return archive_error::invalid_semantic_data;
    }
    const auto feature_values_offset = 16U + variant_count * variant_record_size;
    desc.variants.reserve(variant_count);
    for (std::uint32_t index = 0; index < variant_count; ++index) {
      const auto record = 16U + index * variant_record_size;
      const auto feature_start = read_u32(variant_records, record + 16);
      const auto count = read_u32(variant_records, record + 20);
      const auto shader_start = read_u32(variant_records, record + 24);
      const auto count_shaders = read_u32(variant_records, record + 28);
      if (feature_start > (variant_records.size() - feature_values_offset) / feature_value_size ||
          count > (variant_records.size() - feature_values_offset) / feature_value_size -
                      feature_start ||
          shader_start > shaders.size() || count_shaders > shaders.size() - shader_start ||
          read_u64(variant_records, record + 32) != 0) {
        return archive_error::invalid_semantic_data;
      }
      material_variant_desc variant{
          .pass = read_u64(variant_records, record), .features = {}, .shaders = {}, .pipeline = {}};
      variant.features.reserve(count);
      for (std::uint32_t feature_index = 0; feature_index < count; ++feature_index) {
        const auto feature_record =
            feature_values_offset + (feature_start + feature_index) * feature_value_size;
        const auto id = read_u64(variant_records, feature_record);
        if (!feature_ids.contains(id) || read_u32(variant_records, feature_record + 12) != 0) {
          return archive_error::invalid_semantic_data;
        }
        variant.features.push_back({id, read_u32(variant_records, feature_record + 8)});
      }
      if (make_variant_key(variant.features) != read_u64(variant_records, record + 8)) {
        return archive_error::invalid_semantic_data;
      }
      variant.shaders.insert(variant.shaders.end(), shaders.begin() + shader_start,
                             shaders.begin() + shader_start + count_shaders);
      desc.variants.push_back(std::move(variant));
    }

    const auto pipeline_count = read_u32(pipeline_states, 0);
    const auto pipeline_record_size = read_u32(pipeline_states, 4);
    const auto vertex_buffer_count = read_u32(pipeline_states, 8);
    const auto vertex_buffer_record_size = read_u32(pipeline_states, 12);
    const auto vertex_attribute_count = read_u32(pipeline_states, 16);
    const auto vertex_attribute_record_size = read_u32(pipeline_states, 20);
    if (pipeline_count != variant_count || pipeline_record_size != 80 ||
        vertex_buffer_record_size != 16 || vertex_attribute_record_size != 16 ||
        vertex_buffer_count > variant_count * 16U ||
        vertex_attribute_count > vertex_buffer_count * 32U ||
        !record_range_valid(pipeline_count, pipeline_record_size, 24, pipeline_states.size())) {
      return archive_error::invalid_semantic_data;
    }
    const auto buffer_records_offset = 24U + pipeline_count * pipeline_record_size;
    if (!record_range_valid(vertex_buffer_count, vertex_buffer_record_size, buffer_records_offset,
                            pipeline_states.size())) {
      return archive_error::invalid_semantic_data;
    }
    const auto attribute_records_offset =
        buffer_records_offset + vertex_buffer_count * vertex_buffer_record_size;
    if (!record_range_valid(vertex_attribute_count, vertex_attribute_record_size,
                            attribute_records_offset, pipeline_states.size()) ||
        attribute_records_offset + vertex_attribute_count * vertex_attribute_record_size !=
            pipeline_states.size()) {
      return archive_error::invalid_semantic_data;
    }
    for (std::uint32_t index = 0; index < pipeline_count; ++index) {
      const auto record = 24U + index * pipeline_record_size;
      auto& state = desc.variants[index].pipeline;
      const auto buffer_start = read_u32(pipeline_states, record);
      const auto buffer_count = read_u32(pipeline_states, record + 4);
      if (buffer_start > vertex_buffer_count || buffer_count > vertex_buffer_count - buffer_start ||
          read_u32(pipeline_states, record + 72) != 0 ||
          read_u32(pipeline_states, record + 76) != 0) {
        return archive_error::invalid_semantic_data;
      }
      state.primitive = {
          read_u32(pipeline_states, record + 8), read_u32(pipeline_states, record + 12),
          read_u32(pipeline_states, record + 16), read_u32(pipeline_states, record + 20)};
      state.depth = {read_u32(pipeline_states, record + 24), read_u32(pipeline_states, record + 28),
                     read_u32(pipeline_states, record + 32),
                     read_u32(pipeline_states, record + 36)};
      state.color_blend = {
          read_u32(pipeline_states, record + 40), read_u32(pipeline_states, record + 44),
          read_u32(pipeline_states, record + 48), read_u32(pipeline_states, record + 52),
          read_u32(pipeline_states, record + 56), read_u32(pipeline_states, record + 60),
          read_u32(pipeline_states, record + 64), read_u32(pipeline_states, record + 68)};
      state.vertex_buffers.reserve(buffer_count);
      for (std::uint32_t buffer_index = 0; buffer_index < buffer_count; ++buffer_index) {
        const auto buffer_record =
            buffer_records_offset + (buffer_start + buffer_index) * vertex_buffer_record_size;
        material_vertex_buffer_layout buffer;
        buffer.stride = read_u32(pipeline_states, buffer_record);
        buffer.step_mode = read_u32(pipeline_states, buffer_record + 4);
        const auto attribute_start = read_u32(pipeline_states, buffer_record + 8);
        const auto attribute_count = read_u32(pipeline_states, buffer_record + 12);
        if (attribute_start > vertex_attribute_count ||
            attribute_count > vertex_attribute_count - attribute_start) {
          return archive_error::invalid_semantic_data;
        }
        buffer.attributes.reserve(attribute_count);
        for (std::uint32_t attribute_index = 0; attribute_index < attribute_count;
             ++attribute_index) {
          const auto attribute_record =
              attribute_records_offset +
              (attribute_start + attribute_index) * vertex_attribute_record_size;
          if (read_u32(pipeline_states, attribute_record + 12) != 0) {
            return archive_error::invalid_semantic_data;
          }
          buffer.attributes.push_back({read_u32(pipeline_states, attribute_record),
                                       read_u32(pipeline_states, attribute_record + 4),
                                       read_u32(pipeline_states, attribute_record + 8)});
        }
        state.vertex_buffers.push_back(std::move(buffer));
      }
    }

    const auto pass_count = read_u32(pass_definitions, 0);
    const auto pass_record_size = read_u32(pass_definitions, 4);
    if (pass_count == 0 || pass_count > max_passes || pass_record_size != 16 ||
        !record_range_valid(pass_count, pass_record_size, 8, pass_definitions.size())) {
      return archive_error::invalid_semantic_data;
    }
    std::vector<bool> covered(variant_count);
    for (std::uint32_t index = 0; index < pass_count; ++index) {
      const auto record = 8U + index * pass_record_size;
      const auto pass = read_u64(pass_definitions, record);
      const auto first = read_u32(pass_definitions, record + 8);
      const auto count = read_u32(pass_definitions, record + 12);
      if (pass == 0 || count == 0 || first > variant_count || count > variant_count - first) {
        return archive_error::invalid_semantic_data;
      }
      for (std::uint32_t variant_index = first; variant_index < first + count; ++variant_index) {
        if (covered[variant_index] || desc.variants[variant_index].pass != pass) {
          return archive_error::invalid_semantic_data;
        }
        covered[variant_index] = true;
      }
    }
    if (!std::ranges::all_of(covered, [](bool value) { return value; })) {
      return archive_error::invalid_semantic_data;
    }

    material_package decoded;
    if (material_package::build(std::move(desc), decoded) != package_error::none) {
      return archive_error::invalid_semantic_data;
    }
    package = std::move(decoded);
    return archive_error::none;
  } catch (const std::bad_alloc&) {
    return archive_error::out_of_memory;
  } catch (...) {
    return archive_error::invalid_semantic_data;
  }
}

} // namespace granit::material
