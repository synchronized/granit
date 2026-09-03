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
  case archive_section_type::wgsl_data:
    return "wgsl_data";
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

const char* vertex_format_name(granit_vertex_format format) noexcept {
  switch (format) {
  case GRANIT_VERTEX_FORMAT_FLOAT32:
    return "float32";
  case GRANIT_VERTEX_FORMAT_FLOAT32X2:
    return "float32x2";
  case GRANIT_VERTEX_FORMAT_FLOAT32X3:
    return "float32x3";
  case GRANIT_VERTEX_FORMAT_FLOAT32X4:
    return "float32x4";
  case GRANIT_VERTEX_FORMAT_UINT32:
    return "uint32";
  case GRANIT_VERTEX_FORMAT_UINT32X2:
    return "uint32x2";
  case GRANIT_VERTEX_FORMAT_UINT32X3:
    return "uint32x3";
  case GRANIT_VERTEX_FORMAT_UINT32X4:
    return "uint32x4";
  case GRANIT_VERTEX_FORMAT_SINT32:
    return "sint32";
  case GRANIT_VERTEX_FORMAT_SINT32X2:
    return "sint32x2";
  case GRANIT_VERTEX_FORMAT_SINT32X3:
    return "sint32x3";
  case GRANIT_VERTEX_FORMAT_SINT32X4:
    return "sint32x4";
  default:
    return "unknown";
  }
}

const char* topology_name(granit_primitive_topology value) noexcept {
  switch (value) {
  case GRANIT_PRIMITIVE_TOPOLOGY_POINT_LIST:
    return "point_list";
  case GRANIT_PRIMITIVE_TOPOLOGY_LINE_LIST:
    return "line_list";
  case GRANIT_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    return "line_strip";
  case GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    return "triangle_list";
  case GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    return "triangle_strip";
  default:
    return "unknown";
  }
}

const char* compare_name(granit_compare_operation value) noexcept {
  switch (value) {
  case GRANIT_COMPARE_OPERATION_NEVER:
    return "never";
  case GRANIT_COMPARE_OPERATION_LESS:
    return "less";
  case GRANIT_COMPARE_OPERATION_EQUAL:
    return "equal";
  case GRANIT_COMPARE_OPERATION_LESS_EQUAL:
    return "less_equal";
  case GRANIT_COMPARE_OPERATION_GREATER:
    return "greater";
  case GRANIT_COMPARE_OPERATION_NOT_EQUAL:
    return "not_equal";
  case GRANIT_COMPARE_OPERATION_GREATER_EQUAL:
    return "greater_equal";
  case GRANIT_COMPARE_OPERATION_ALWAYS:
    return "always";
  default:
    return "unknown";
  }
}

const char* cull_mode_name(granit_cull_mode value) noexcept {
  switch (value) {
  case GRANIT_CULL_MODE_NONE:
    return "none";
  case GRANIT_CULL_MODE_FRONT:
    return "front";
  case GRANIT_CULL_MODE_BACK:
    return "back";
  case GRANIT_CULL_MODE_FRONT_AND_BACK:
    return "front_and_back";
  default:
    return "unknown";
  }
}

const char* polygon_mode_name(granit_polygon_mode value) noexcept {
  switch (value) {
  case GRANIT_POLYGON_MODE_FILL:
    return "fill";
  case GRANIT_POLYGON_MODE_LINE:
    return "line";
  case GRANIT_POLYGON_MODE_POINT:
    return "point";
  default:
    return "unknown";
  }
}

const char* blend_factor_name(granit_blend_factor value) noexcept {
  switch (value) {
  case GRANIT_BLEND_FACTOR_ZERO:
    return "zero";
  case GRANIT_BLEND_FACTOR_ONE:
    return "one";
  case GRANIT_BLEND_FACTOR_SOURCE_COLOR:
    return "source_color";
  case GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR:
    return "one_minus_source_color";
  case GRANIT_BLEND_FACTOR_SOURCE_ALPHA:
    return "source_alpha";
  case GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA:
    return "one_minus_source_alpha";
  case GRANIT_BLEND_FACTOR_DESTINATION_COLOR:
    return "destination_color";
  case GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR:
    return "one_minus_destination_color";
  case GRANIT_BLEND_FACTOR_DESTINATION_ALPHA:
    return "destination_alpha";
  case GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA:
    return "one_minus_destination_alpha";
  default:
    return "unknown";
  }
}

const char* blend_operation_name(granit_blend_operation value) noexcept {
  switch (value) {
  case GRANIT_BLEND_OPERATION_ADD:
    return "add";
  case GRANIT_BLEND_OPERATION_SUBTRACT:
    return "subtract";
  case GRANIT_BLEND_OPERATION_REVERSE_SUBTRACT:
    return "reverse_subtract";
  case GRANIT_BLEND_OPERATION_MIN:
    return "min";
  case GRANIT_BLEND_OPERATION_MAX:
    return "max";
  default:
    return "unknown";
  }
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
    stream << "\n  },\n  \"requirements\": {\n    \"target_environment\": \"cross_backend\",\n"
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
        stream << ", \"spirv_size\": " << shader.spirv.size() * sizeof(std::uint32_t)
               << ", \"wgsl_size\": " << shader.wgsl.size() << '}';
      }
      stream << "], \"pipeline\": {\"vertex_buffers\": [";
      for (std::size_t buffer_index = 0; buffer_index < variant.pipeline.vertex_buffers.size();
           ++buffer_index) {
        const auto& buffer = variant.pipeline.vertex_buffers[buffer_index];
        stream << (buffer_index == 0 ? "" : ", ") << "{\"stride\": " << buffer.stride
               << ", \"step_mode\": \""
               << (buffer.step_mode == GRANIT_VERTEX_STEP_MODE_VERTEX ? "vertex" : "instance")
               << "\", \"attributes\": [";
        for (std::size_t attribute_index = 0; attribute_index < buffer.attributes.size();
             ++attribute_index) {
          const auto& attribute = buffer.attributes[attribute_index];
          stream << (attribute_index == 0 ? "" : ", ") << "{\"location\": " << attribute.location
                 << ", \"format\": \"" << vertex_format_name(attribute.format)
                 << "\", \"offset\": " << attribute.offset << '}';
        }
        stream << "]}";
      }
      stream << "], \"primitive\": {\"topology\": \""
             << topology_name(variant.pipeline.primitive.topology) << "\", \"front_face\": \""
             << (variant.pipeline.primitive.front_face == GRANIT_FRONT_FACE_COUNTER_CLOCKWISE
                     ? "counter_clockwise"
                     : "clockwise")
             << "\", \"cull_mode\": \"" << cull_mode_name(variant.pipeline.primitive.cull_mode)
             << "\", \"polygon_mode\": \""
             << polygon_mode_name(variant.pipeline.primitive.polygon_mode) << "\""
             << "}, \"depth\": {\"test_enabled\": "
             << (variant.pipeline.depth.test_enabled != 0 ? "true" : "false")
             << ", \"write_enabled\": "
             << (variant.pipeline.depth.write_enabled != 0 ? "true" : "false")
             << ", \"compare\": \"" << compare_name(variant.pipeline.depth.compare)
             << "\"}, \"color_blend\": {\"enabled\": "
             << (variant.pipeline.color_blend.enabled != 0 ? "true" : "false")
             << ", \"source_color_factor\": \""
             << blend_factor_name(variant.pipeline.color_blend.source_color_factor)
             << "\", \"destination_color_factor\": \""
             << blend_factor_name(variant.pipeline.color_blend.destination_color_factor)
             << "\", \"color_operation\": \""
             << blend_operation_name(variant.pipeline.color_blend.color_operation)
             << "\", \"source_alpha_factor\": \""
             << blend_factor_name(variant.pipeline.color_blend.source_alpha_factor)
             << "\", \"destination_alpha_factor\": \""
             << blend_factor_name(variant.pipeline.color_blend.destination_alpha_factor)
             << "\", \"alpha_operation\": \""
             << blend_operation_name(variant.pipeline.color_blend.alpha_operation) << "\""
             << ", \"write_mask\": " << variant.pipeline.color_blend.write_mask << "}}}";
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
