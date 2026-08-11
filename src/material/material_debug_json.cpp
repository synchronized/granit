// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_debug_json.h"

#include "material/material_package_archive.h"

#include <iomanip>
#include <new>
#include <sstream>

namespace granit::material {
namespace {

const char* section_name(std::uint32_t type) noexcept {
  switch (static_cast<archive_section_type>(type)) {
  case archive_section_type::string_table:
    return "string_table";
  case archive_section_type::material_metadata:
    return "material_metadata";
  case archive_section_type::feature_definitions:
    return "feature_definitions";
  case archive_section_type::pass_definitions:
    return "pass_definitions";
  case archive_section_type::variant_records:
    return "variant_records";
  case archive_section_type::shader_records:
    return "shader_records";
  case archive_section_type::spirv_data:
    return "spirv_data";
  case archive_section_type::build_metadata:
    return "build_metadata";
  case archive_section_type::dependency_metadata:
    return "dependency_metadata";
  case archive_section_type::pipeline_states:
    return "pipeline_states";
  }
  return "unknown";
}

const char* parameter_type_name(parameter_type type) noexcept {
  switch (type) {
  case parameter_type::bool32:
    return "bool32";
  case parameter_type::int32:
    return "int32";
  case parameter_type::uint32:
    return "uint32";
  case parameter_type::float32:
    return "float32";
  case parameter_type::float2:
    return "float2";
  case parameter_type::float3:
    return "float3";
  case parameter_type::float4:
    return "float4";
  case parameter_type::matrix4:
    return "matrix4";
  case parameter_type::texture_view:
    return "texture_view";
  case parameter_type::sampler:
    return "sampler";
  }
  return "unknown";
}

const char* shader_stage_name(package_shader_stage stage) noexcept {
  return stage == package_shader_stage::vertex ? "vertex" : "fragment";
}

void write_json_string(std::ostream& stream, std::string_view value) {
  stream << '"';
  for (const auto character : value) {
    const auto byte = static_cast<unsigned char>(character);
    switch (character) {
    case '"':
      stream << "\\\"";
      break;
    case '\\':
      stream << "\\\\";
      break;
    case '\b':
      stream << "\\b";
      break;
    case '\f':
      stream << "\\f";
      break;
    case '\n':
      stream << "\\n";
      break;
    case '\r':
      stream << "\\r";
      break;
    case '\t':
      stream << "\\t";
      break;
    default:
      if (byte < 0x20U) {
        stream << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned>(byte) << std::dec;
      } else {
        stream << character;
      }
    }
  }
  stream << '"';
}

void write_hex_u64(std::ostream& stream, std::uint64_t value) {
  stream << '"' << "0x" << std::hex << std::setw(16) << std::setfill('0') << value << std::dec
         << '"';
}

void write_hash(std::ostream& stream, const material_archive_hash& hash) {
  stream << '"';
  for (const auto value : hash) {
    stream << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<unsigned>(std::to_integer<std::uint8_t>(value));
  }
  stream << std::dec << '"';
}

} // namespace

archive_error export_material_archive_debug_json(std::span<const std::byte> bytes,
                                                 std::string& json) noexcept {
  material_archive_layout layout;
  auto result = parse_material_archive_layout(bytes, layout);
  if (result != archive_error::none) {
    return result;
  }
  material_package package;
  result = decode_material_package_archive(bytes, package);
  if (result != archive_error::none) {
    return result;
  }
  try {
    std::ostringstream stream;
    stream << "{\n  \"format\": {\n    \"magic\": \"GRMAT\",\n    \"version_major\": "
           << layout.header.version_major
           << ",\n    \"version_minor\": " << layout.header.version_minor
           << ",\n    \"file_size\": " << layout.header.file_size << ",\n    \"content_hash\": ";
    write_hash(stream, layout.header.content_hash);
    stream << "\n  },\n  \"requirements\": {\n    \"target_environment\": \"vulkan1.3\",\n"
              "    \"binding_model\": \"bind_group\",\n    \"renderer_features\": []\n  },\n"
              "  \"sections\": [";
    for (std::size_t index = 0; index < layout.sections.size(); ++index) {
      const auto& section = layout.sections[index];
      stream << (index == 0 ? "\n" : ",\n") << "    {\"type\": ";
      write_json_string(stream, section_name(section.type));
      stream << ", \"offset\": " << section.offset << ", \"size\": " << section.stored_size
             << ", \"required\": "
             << ((section.flags & archive_section_required) != 0 ? "true" : "false") << '}';
    }
    stream << "\n  ],\n  \"material\": {\n    \"constant_buffer_size\": "
           << package.metadata().constant_buffer_size() << ",\n    \"parameters\": [";
    const auto parameters = package.metadata().parameters();
    for (std::size_t index = 0; index < parameters.size(); ++index) {
      const auto& parameter = parameters[index];
      stream << (index == 0 ? "\n" : ",\n") << "      {\"id\": ";
      write_hex_u64(stream, parameter.id);
      stream << ", \"name\": ";
      write_json_string(stream, parameter.name);
      stream << ", \"type\": ";
      write_json_string(stream, parameter_type_name(parameter.type));
      stream << ", \"offset\": " << parameter.offset
             << ", \"array_count\": " << parameter.array_count
             << ", \"array_stride\": " << parameter.array_stride
             << ", \"binding\": " << parameter.binding
             << ", \"default_size\": " << parameter.default_value.size() << '}';
    }
    stream << "\n    ],\n    \"variants\": [";
    const auto variants = package.variants();
    for (std::size_t index = 0; index < variants.size(); ++index) {
      const auto& variant = variants[index];
      stream << (index == 0 ? "\n" : ",\n") << "      {\"pass_id\": ";
      write_hex_u64(stream, variant.pass);
      stream << ", \"key\": ";
      write_hex_u64(stream, variant.key);
      stream << ", \"features\": [";
      for (std::size_t feature_index = 0; feature_index < variant.features.size();
           ++feature_index) {
        const auto& feature = variant.features[feature_index];
        stream << (feature_index == 0 ? "" : ", ") << "{\"id\": ";
        write_hex_u64(stream, feature.id);
        stream << ", \"value\": " << feature.value << '}';
      }
      stream << "], \"shaders\": [";
      for (std::size_t shader_index = 0; shader_index < variant.shaders.size(); ++shader_index) {
        const auto& shader = variant.shaders[shader_index];
        stream << (shader_index == 0 ? "" : ", ") << "{\"stage\": ";
        write_json_string(stream, shader_stage_name(shader.stage));
        stream << ", \"entry_point\": ";
        write_json_string(stream, shader.entry_point);
        stream << ", \"spirv_size\": " << shader.spirv.size() * sizeof(std::uint32_t) << '}';
      }
      stream << "]}";
    }
    stream << "\n    ]\n  }\n}\n";
    json = std::move(stream).str();
    return archive_error::none;
  } catch (const std::bad_alloc&) {
    return archive_error::out_of_memory;
  } catch (...) {
    return archive_error::invalid_semantic_data;
  }
}

} // namespace granit::material
