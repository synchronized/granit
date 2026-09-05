// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_source_json.h"
#include "assets/shader_asset.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace granit::material {
namespace {

struct json_value {
  using object = std::map<std::string, json_value, std::less<>>;
  using array = std::vector<json_value>;
  std::variant<std::nullptr_t, bool, std::uint64_t, std::string, object, array> data;
};

class json_parser {
public:
  explicit json_parser(std::string_view text) : text_(text) {}

  bool parse(json_value& value) {
    skip_space();
    if (!parse_value(value, 0)) {
      return false;
    }
    skip_space();
    return position_ == text_.size();
  }

private:
  void skip_space() {
    while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\n' ||
                                        text_[position_] == '\r' || text_[position_] == '\t')) {
      ++position_;
    }
  }

  bool consume(char expected) {
    skip_space();
    if (position_ >= text_.size() || text_[position_] != expected) {
      return false;
    }
    ++position_;
    return true;
  }

  bool parse_value(json_value& value, std::size_t depth) {
    skip_space();
    if (position_ >= text_.size() || depth > material_source_json_max_depth) {
      return false;
    }
    if (text_[position_] == '{') {
      json_value::object object;
      if (!parse_object(object, depth + 1)) {
        return false;
      }
      value.data = std::move(object);
      return true;
    }
    if (text_[position_] == '[') {
      json_value::array array;
      if (!parse_array(array, depth + 1)) {
        return false;
      }
      value.data = std::move(array);
      return true;
    }
    if (text_[position_] == '"') {
      std::string string;
      if (!parse_string(string)) {
        return false;
      }
      value.data = std::move(string);
      return true;
    }
    if (text_[position_] >= '0' && text_[position_] <= '9') {
      return parse_number(value);
    }
    for (const auto& [token, result] :
         {std::pair<std::string_view, json_value>{"true", json_value{true}},
          {"false", json_value{false}},
          {"null", json_value{nullptr}}}) {
      if (text_.substr(position_).starts_with(token)) {
        position_ += token.size();
        value = result;
        return true;
      }
    }
    return false;
  }

  bool parse_object(json_value::object& object, std::size_t depth) {
    if (!consume('{')) {
      return false;
    }
    skip_space();
    if (position_ < text_.size() && text_[position_] == '}') {
      ++position_;
      return true;
    }
    while (true) {
      std::string key;
      if (!parse_string(key) || !consume(':')) {
        return false;
      }
      json_value value;
      if (!parse_value(value, depth) || !object.emplace(std::move(key), std::move(value)).second) {
        return false;
      }
      skip_space();
      if (position_ < text_.size() && text_[position_] == '}') {
        ++position_;
        return true;
      }
      if (!consume(',')) {
        return false;
      }
    }
  }

  bool parse_array(json_value::array& array, std::size_t depth) {
    if (!consume('[')) {
      return false;
    }
    skip_space();
    if (position_ < text_.size() && text_[position_] == ']') {
      ++position_;
      return true;
    }
    while (true) {
      json_value value;
      if (!parse_value(value, depth)) {
        return false;
      }
      array.push_back(std::move(value));
      skip_space();
      if (position_ < text_.size() && text_[position_] == ']') {
        ++position_;
        return true;
      }
      if (!consume(',')) {
        return false;
      }
    }
  }

  bool parse_string(std::string& value) {
    skip_space();
    if (position_ >= text_.size() || text_[position_++] != '"') {
      return false;
    }
    while (position_ < text_.size()) {
      const char character = text_[position_++];
      if (character == '"') {
        return true;
      }
      if (static_cast<unsigned char>(character) < 0x20U) {
        return false;
      }
      if (character != '\\') {
        value.push_back(character);
        continue;
      }
      if (position_ >= text_.size()) {
        return false;
      }
      const char escaped = text_[position_++];
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        value.push_back(escaped);
        break;
      case 'b':
        value.push_back('\b');
        break;
      case 'f':
        value.push_back('\f');
        break;
      case 'n':
        value.push_back('\n');
        break;
      case 'r':
        value.push_back('\r');
        break;
      case 't':
        value.push_back('\t');
        break;
      default:
        return false; // 首版不接受 Unicode 转义，源文件直接使用 UTF-8。
      }
    }
    return false;
  }

  bool parse_number(json_value& value) {
    const auto begin = position_;
    while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
      ++position_;
    }
    std::uint64_t number = 0;
    const auto result = std::from_chars(text_.data() + begin, text_.data() + position_, number);
    if (result.ec != std::errc{}) {
      return false;
    }
    value.data = number;
    return true;
  }

  std::string_view text_;
  std::size_t position_ = 0;
};

const json_value* member(const json_value::object& object, std::string_view name) {
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &found->second;
}

template <typename T> const T* as(const json_value* value) {
  return value == nullptr ? nullptr : std::get_if<T>(&value->data);
}

bool u32(const json_value* value, std::uint32_t& result) {
  const auto* number = as<std::uint64_t>(value);
  if (number == nullptr || *number > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  result = static_cast<std::uint32_t>(*number);
  return true;
}

bool read_shader_asset(const std::filesystem::path& path, material_shader_code& code) {
  constexpr std::uint64_t maximum_size = UINT64_C(16) * 1024 * 1024;
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    return false;
  const auto length = stream.tellg();
  if (length <= 0 || static_cast<std::uint64_t>(length) > maximum_size)
    return false;
  std::vector<std::byte> bytes(static_cast<std::size_t>(length));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), length);
  if (!stream)
    return false;
  granit::tools::shader_asset_view asset;
  if (granit::tools::decode_shader_asset(bytes, asset) !=
      granit::tools::shader_asset_error::success) {
    return false;
  }
  if (asset.stage == GRANIT_SHADER_STAGE_VERTEX) {
    code.stage = package_shader_stage::vertex;
  } else if (asset.stage == GRANIT_SHADER_STAGE_FRAGMENT) {
    code.stage = package_shader_stage::fragment;
  } else {
    return false;
  }
  code.entry_point = asset.entry_point;
  code.asset_id = asset.content_id;
  return true;
}

bool parse_parameter(const json_value& value, parameter_desc& parameter) {
  const auto* object = as<json_value::object>(&value);
  const auto* name = object == nullptr ? nullptr : as<std::string>(member(*object, "name"));
  const auto* type = object == nullptr ? nullptr : as<std::string>(member(*object, "type"));
  if (name == nullptr || type == nullptr) {
    return false;
  }
  static const std::map<std::string_view, parameter_type> types = {
      {"bool32", parameter_type::bool32},
      {"int32", parameter_type::int32},
      {"uint32", parameter_type::uint32},
      {"float32", parameter_type::float32},
      {"float2", parameter_type::float2},
      {"float3", parameter_type::float3},
      {"float4", parameter_type::float4},
      {"matrix4", parameter_type::matrix4},
      {"texture_view", parameter_type::texture_view},
      {"sampler", parameter_type::sampler}};
  const auto found = types.find(*type);
  if (found == types.end()) {
    return false;
  }
  parameter.name = *name;
  parameter.id = make_parameter_id(*name);
  parameter.type = found->second;
  if (const auto* offset = member(*object, "offset");
      offset != nullptr && !u32(offset, parameter.offset)) {
    return false;
  }
  if (const auto* count = member(*object, "array_count");
      count != nullptr && !u32(count, parameter.array_count)) {
    return false;
  }
  if (const auto* stride = member(*object, "array_stride");
      stride != nullptr && !u32(stride, parameter.array_stride)) {
    return false;
  }
  if (const auto* binding = member(*object, "binding");
      binding != nullptr && !u32(binding, parameter.binding)) {
    return false;
  }
  if (const auto* defaults = member(*object, "default_bytes"); defaults != nullptr) {
    const auto* array = as<json_value::array>(defaults);
    if (array == nullptr) {
      return false;
    }
    for (const auto& item : *array) {
      const auto* byte = as<std::uint64_t>(&item);
      if (byte == nullptr || *byte > 255) {
        return false;
      }
      parameter.default_value.push_back(static_cast<std::byte>(*byte));
    }
  }
  return true;
}

template <typename Value, std::size_t Size>
bool parse_named_value(const json_value::object& object, std::string_view field,
                       const std::array<std::pair<std::string_view, Value>, Size>& values,
                       Value& result) {
  const auto* name = as<std::string>(member(object, field));
  if (name == nullptr) {
    return false;
  }
  const auto found = std::ranges::find_if(
      values, [&](const auto& entry) { return entry.first == std::string_view{*name}; });
  if (found == values.end()) {
    return false;
  }
  result = found->second;
  return true;
}

template <typename Value, std::size_t Size>
bool parse_optional_named_value(const json_value::object& object, std::string_view field,
                                const std::array<std::pair<std::string_view, Value>, Size>& values,
                                Value& result) {
  return member(object, field) == nullptr || parse_named_value(object, field, values, result);
}

bool parse_pipeline(const json_value& value, material_pipeline_state& pipeline) {
  const auto* object = as<json_value::object>(&value);
  if (object == nullptr) {
    return false;
  }
  if (const auto* buffers_value = member(*object, "vertex_buffers"); buffers_value != nullptr) {
    const auto* buffers = as<json_value::array>(buffers_value);
    if (buffers == nullptr)
      return false;
    constexpr std::array<std::pair<std::string_view, granit_vertex_step_mode>, 2> step_modes{
        {{"vertex", GRANIT_VERTEX_STEP_MODE_VERTEX},
         {"instance", GRANIT_VERTEX_STEP_MODE_INSTANCE}}};
    constexpr std::array<std::pair<std::string_view, granit_vertex_format>, 12> formats{
        {{"float32", GRANIT_VERTEX_FORMAT_FLOAT32},
         {"float32x2", GRANIT_VERTEX_FORMAT_FLOAT32X2},
         {"float32x3", GRANIT_VERTEX_FORMAT_FLOAT32X3},
         {"float32x4", GRANIT_VERTEX_FORMAT_FLOAT32X4},
         {"uint32", GRANIT_VERTEX_FORMAT_UINT32},
         {"uint32x2", GRANIT_VERTEX_FORMAT_UINT32X2},
         {"uint32x3", GRANIT_VERTEX_FORMAT_UINT32X3},
         {"uint32x4", GRANIT_VERTEX_FORMAT_UINT32X4},
         {"sint32", GRANIT_VERTEX_FORMAT_SINT32},
         {"sint32x2", GRANIT_VERTEX_FORMAT_SINT32X2},
         {"sint32x3", GRANIT_VERTEX_FORMAT_SINT32X3},
         {"sint32x4", GRANIT_VERTEX_FORMAT_SINT32X4}}};
    for (const auto& buffer_value : *buffers) {
      const auto* buffer_object = as<json_value::object>(&buffer_value);
      material_vertex_buffer_layout buffer;
      const auto* attributes = buffer_object == nullptr
                                   ? nullptr
                                   : as<json_value::array>(member(*buffer_object, "attributes"));
      if (buffer_object == nullptr || !u32(member(*buffer_object, "stride"), buffer.stride) ||
          !parse_named_value(*buffer_object, "step_mode", step_modes, buffer.step_mode) ||
          attributes == nullptr)
        return false;
      for (const auto& attribute_value : *attributes) {
        const auto* attribute_object = as<json_value::object>(&attribute_value);
        material_vertex_attribute attribute;
        if (attribute_object == nullptr ||
            !u32(member(*attribute_object, "location"), attribute.location) ||
            !parse_named_value(*attribute_object, "format", formats, attribute.format) ||
            !u32(member(*attribute_object, "offset"), attribute.offset))
          return false;
        buffer.attributes.push_back(attribute);
      }
      pipeline.vertex_buffers.push_back(std::move(buffer));
    }
  }
  constexpr std::array<std::pair<std::string_view, granit_primitive_topology>, 5> topologies{
      {{"point_list", GRANIT_PRIMITIVE_TOPOLOGY_POINT_LIST},
       {"line_list", GRANIT_PRIMITIVE_TOPOLOGY_LINE_LIST},
       {"line_strip", GRANIT_PRIMITIVE_TOPOLOGY_LINE_STRIP},
       {"triangle_list", GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
       {"triangle_strip", GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP}}};
  constexpr std::array<std::pair<std::string_view, granit_front_face>, 2> front_faces{
      {{"counter_clockwise", GRANIT_FRONT_FACE_COUNTER_CLOCKWISE},
       {"clockwise", GRANIT_FRONT_FACE_CLOCKWISE}}};
  constexpr std::array<std::pair<std::string_view, granit_cull_mode>, 4> cull_modes{
      {{"none", GRANIT_CULL_MODE_NONE},
       {"front", GRANIT_CULL_MODE_FRONT},
       {"back", GRANIT_CULL_MODE_BACK},
       {"front_and_back", GRANIT_CULL_MODE_FRONT_AND_BACK}}};
  constexpr std::array<std::pair<std::string_view, granit_polygon_mode>, 3> polygon_modes{
      {{"fill", GRANIT_POLYGON_MODE_FILL},
       {"line", GRANIT_POLYGON_MODE_LINE},
       {"point", GRANIT_POLYGON_MODE_POINT}}};
  if (const auto* primitive_value = member(*object, "primitive"); primitive_value != nullptr) {
    const auto* primitive = as<json_value::object>(primitive_value);
    if (primitive == nullptr ||
        !parse_named_value(*primitive, "topology", topologies, pipeline.primitive.topology) ||
        !parse_named_value(*primitive, "front_face", front_faces, pipeline.primitive.front_face) ||
        !parse_named_value(*primitive, "cull_mode", cull_modes, pipeline.primitive.cull_mode) ||
        !parse_named_value(*primitive, "polygon_mode", polygon_modes,
                           pipeline.primitive.polygon_mode))
      return false;
  }
  constexpr std::array<std::pair<std::string_view, granit_compare_operation>, 8> comparisons{
      {{"never", GRANIT_COMPARE_OPERATION_NEVER},
       {"less", GRANIT_COMPARE_OPERATION_LESS},
       {"equal", GRANIT_COMPARE_OPERATION_EQUAL},
       {"less_equal", GRANIT_COMPARE_OPERATION_LESS_EQUAL},
       {"greater", GRANIT_COMPARE_OPERATION_GREATER},
       {"not_equal", GRANIT_COMPARE_OPERATION_NOT_EQUAL},
       {"greater_equal", GRANIT_COMPARE_OPERATION_GREATER_EQUAL},
       {"always", GRANIT_COMPARE_OPERATION_ALWAYS}}};
  if (const auto* depth_value = member(*object, "depth"); depth_value != nullptr) {
    const auto* depth = as<json_value::object>(depth_value);
    const auto* test = depth == nullptr ? nullptr : as<bool>(member(*depth, "test_enabled"));
    const auto* write = depth == nullptr ? nullptr : as<bool>(member(*depth, "write_enabled"));
    if (depth == nullptr || test == nullptr || write == nullptr ||
        !parse_named_value(*depth, "compare", comparisons, pipeline.depth.compare))
      return false;
    pipeline.depth.test_enabled = *test ? 1U : 0U;
    pipeline.depth.write_enabled = *write ? 1U : 0U;
  }
  if (const auto* blend_value = member(*object, "color_blend"); blend_value != nullptr) {
    const auto* blend = as<json_value::object>(blend_value);
    const auto* enabled = blend == nullptr ? nullptr : as<bool>(member(*blend, "enabled"));
    constexpr std::array<std::pair<std::string_view, granit_blend_factor>, 10> factors{
        {{"zero", GRANIT_BLEND_FACTOR_ZERO},
         {"one", GRANIT_BLEND_FACTOR_ONE},
         {"source_color", GRANIT_BLEND_FACTOR_SOURCE_COLOR},
         {"one_minus_source_color", GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR},
         {"source_alpha", GRANIT_BLEND_FACTOR_SOURCE_ALPHA},
         {"one_minus_source_alpha", GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA},
         {"destination_color", GRANIT_BLEND_FACTOR_DESTINATION_COLOR},
         {"one_minus_destination_color", GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR},
         {"destination_alpha", GRANIT_BLEND_FACTOR_DESTINATION_ALPHA},
         {"one_minus_destination_alpha", GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA}}};
    constexpr std::array<std::pair<std::string_view, granit_blend_operation>, 5> operations{
        {{"add", GRANIT_BLEND_OPERATION_ADD},
         {"subtract", GRANIT_BLEND_OPERATION_SUBTRACT},
         {"reverse_subtract", GRANIT_BLEND_OPERATION_REVERSE_SUBTRACT},
         {"min", GRANIT_BLEND_OPERATION_MIN},
         {"max", GRANIT_BLEND_OPERATION_MAX}}};
    if (enabled == nullptr ||
        !parse_optional_named_value(*blend, "source_color_factor", factors,
                                    pipeline.color_blend.source_color_factor) ||
        !parse_optional_named_value(*blend, "destination_color_factor", factors,
                                    pipeline.color_blend.destination_color_factor) ||
        !parse_optional_named_value(*blend, "color_operation", operations,
                                    pipeline.color_blend.color_operation) ||
        !parse_optional_named_value(*blend, "source_alpha_factor", factors,
                                    pipeline.color_blend.source_alpha_factor) ||
        !parse_optional_named_value(*blend, "destination_alpha_factor", factors,
                                    pipeline.color_blend.destination_alpha_factor) ||
        !parse_optional_named_value(*blend, "alpha_operation", operations,
                                    pipeline.color_blend.alpha_operation))
      return false;
    pipeline.color_blend.enabled = *enabled ? 1U : 0U;
    if (const auto* write_mask = member(*blend, "write_mask");
        write_mask != nullptr && !u32(write_mask, pipeline.color_blend.write_mask)) {
      return false;
    }
  }
  return true;
}

source_json_error parse_variant(const json_value& value, const std::filesystem::path& directory,
                                material_variant_desc& variant) {
  const auto* object = as<json_value::object>(&value);
  const auto* pass = object == nullptr ? nullptr : as<std::string>(member(*object, "pass"));
  const auto* shaders =
      object == nullptr ? nullptr : as<json_value::array>(member(*object, "shaders"));
  if (pass == nullptr || shaders == nullptr) {
    return source_json_error::invalid_schema;
  }
  variant.pass = make_feature_id(*pass);
  if (const auto* pipeline = member(*object, "pipeline");
      pipeline != nullptr && !parse_pipeline(*pipeline, variant.pipeline)) {
    return source_json_error::invalid_schema;
  }
  if (const auto* features_value = member(*object, "features"); features_value != nullptr) {
    const auto* features = as<json_value::array>(features_value);
    if (features == nullptr) {
      return source_json_error::invalid_schema;
    }
    for (const auto& feature_value : *features) {
      const auto* feature = as<json_value::object>(&feature_value);
      const auto* name = feature == nullptr ? nullptr : as<std::string>(member(*feature, "name"));
      std::uint32_t number = 0;
      if (name == nullptr ||
          !u32(feature == nullptr ? nullptr : member(*feature, "value"), number)) {
        return source_json_error::invalid_schema;
      }
      variant.features.push_back({make_feature_id(*name), number});
    }
  }
  for (const auto& shader_value : *shaders) {
    const auto* shader = as<json_value::object>(&shader_value);
    const auto* asset_path =
        shader == nullptr ? nullptr : as<std::string>(member(*shader, "asset"));
    if (asset_path == nullptr) {
      return source_json_error::invalid_schema;
    }
    material_shader_code code;
    if (!read_shader_asset(directory / std::filesystem::path{*asset_path}, code)) {
      return source_json_error::referenced_file_error;
    }
    variant.shaders.push_back(std::move(code));
  }
  return source_json_error::none;
}

} // namespace

source_json_error parse_material_source_json(std::string_view json,
                                             const std::filesystem::path& source_directory,
                                             material_package& package) noexcept {
  try {
    if (json.empty() || json.size() > material_source_json_max_size) {
      return source_json_error::invalid_json;
    }
    json_value root;
    if (!json_parser{json}.parse(root)) {
      return source_json_error::invalid_json;
    }
    const auto* object = as<json_value::object>(&root);
    std::uint32_t version = 0;
    const auto* target =
        object == nullptr ? nullptr : as<std::string>(member(*object, "target_environment"));
    const auto* binding =
        object == nullptr ? nullptr : as<std::string>(member(*object, "binding_model"));
    const auto* material =
        object == nullptr ? nullptr : as<json_value::object>(member(*object, "material"));
    const auto* variants =
        object == nullptr ? nullptr : as<json_value::array>(member(*object, "variants"));
    if (object == nullptr || !u32(member(*object, "format_version"), version) ||
        target == nullptr || binding == nullptr || material == nullptr || variants == nullptr) {
      return source_json_error::invalid_schema;
    }
    if (version != material_package_format_version || *target != "cross_backend" ||
        *binding != "bind_group") {
      return source_json_error::unsupported_value;
    }
    material_package_desc desc;
    desc.format_version = version;
    if (!u32(member(*material, "constant_buffer_size"), desc.metadata.constant_buffer_size)) {
      return source_json_error::invalid_schema;
    }
    const auto* parameters = as<json_value::array>(member(*material, "parameters"));
    if (parameters == nullptr) {
      return source_json_error::invalid_schema;
    }
    for (const auto& value : *parameters) {
      parameter_desc parameter;
      if (!parse_parameter(value, parameter)) {
        return source_json_error::invalid_schema;
      }
      desc.metadata.parameters.push_back(std::move(parameter));
    }
    for (const auto& value : *variants) {
      material_variant_desc variant;
      const auto result = parse_variant(value, source_directory, variant);
      if (result != source_json_error::none) {
        return result;
      }
      desc.variants.push_back(std::move(variant));
    }
    return material_package::build(std::move(desc), package) == package_error::none
               ? source_json_error::none
               : source_json_error::invalid_package;
  } catch (...) {
    return source_json_error::invalid_schema;
  }
}

} // namespace granit::material
