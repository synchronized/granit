// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_tools/shader/library_source_manifest.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <span>
#include <sstream>
#include <utility>
#include <variant>

namespace granit::asset_tools::detail {
namespace {

constexpr std::size_t maximum_json_size = 4U * 1024U * 1024U;
constexpr std::size_t maximum_json_depth = 32;
constexpr std::size_t maximum_shader_count = 4096;
constexpr std::size_t maximum_variant_count = 4096;
constexpr std::size_t maximum_define_count = 256;

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
    if (!parse_value(value, 0))
      return false;
    skip_space();
    return position_ == text_.size();
  }

private:
  void skip_space() {
    while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\n' ||
                                        text_[position_] == '\r' || text_[position_] == '\t'))
      ++position_;
  }

  bool consume(char expected) {
    skip_space();
    if (position_ >= text_.size() || text_[position_] != expected)
      return false;
    ++position_;
    return true;
  }

  bool parse_value(json_value& value, std::size_t depth) {
    skip_space();
    if (position_ >= text_.size() || depth > maximum_json_depth)
      return false;
    if (text_[position_] == '{') {
      json_value::object object;
      if (!parse_object(object, depth + 1))
        return false;
      value.data = std::move(object);
      return true;
    }
    if (text_[position_] == '[') {
      json_value::array array;
      if (!parse_array(array, depth + 1))
        return false;
      value.data = std::move(array);
      return true;
    }
    if (text_[position_] == '"') {
      std::string string;
      if (!parse_string(string))
        return false;
      value.data = std::move(string);
      return true;
    }
    if (text_[position_] >= '0' && text_[position_] <= '9')
      return parse_number(value);
    for (const auto& [token, replacement] :
         {std::pair<std::string_view, json_value>{"true", json_value{true}},
          {"false", json_value{false}},
          {"null", json_value{nullptr}}}) {
      if (text_.substr(position_).starts_with(token)) {
        position_ += token.size();
        value = replacement;
        return true;
      }
    }
    return false;
  }

  bool parse_object(json_value::object& object, std::size_t depth) {
    if (!consume('{'))
      return false;
    skip_space();
    if (position_ < text_.size() && text_[position_] == '}') {
      ++position_;
      return true;
    }
    while (true) {
      std::string key;
      json_value value;
      if (!parse_string(key) || !consume(':') || !parse_value(value, depth) ||
          !object.emplace(std::move(key), std::move(value)).second)
        return false;
      skip_space();
      if (position_ < text_.size() && text_[position_] == '}') {
        ++position_;
        return true;
      }
      if (!consume(','))
        return false;
    }
  }

  bool parse_array(json_value::array& array, std::size_t depth) {
    if (!consume('['))
      return false;
    skip_space();
    if (position_ < text_.size() && text_[position_] == ']') {
      ++position_;
      return true;
    }
    while (true) {
      json_value value;
      if (!parse_value(value, depth))
        return false;
      array.push_back(std::move(value));
      skip_space();
      if (position_ < text_.size() && text_[position_] == ']') {
        ++position_;
        return true;
      }
      if (!consume(','))
        return false;
    }
  }

  bool parse_string(std::string& value) {
    skip_space();
    if (position_ >= text_.size() || text_[position_++] != '"')
      return false;
    while (position_ < text_.size()) {
      const auto character = text_[position_++];
      if (character == '"')
        return true;
      if (static_cast<unsigned char>(character) < 0x20U)
        return false;
      if (character != '\\') {
        value.push_back(character);
        continue;
      }
      if (position_ >= text_.size())
        return false;
      const auto escaped = text_[position_++];
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
        return false;
      }
    }
    return false;
  }

  bool parse_number(json_value& value) {
    const auto begin = position_;
    while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
      ++position_;
    std::uint64_t number = 0;
    const auto result = std::from_chars(text_.data() + begin, text_.data() + position_, number);
    if (result.ec != std::errc{})
      return false;
    value.data = number;
    return true;
  }

  std::string_view text_;
  std::size_t position_{};
};

template <typename T> const T* as(const json_value* value) {
  return value == nullptr ? nullptr : std::get_if<T>(&value->data);
}

const json_value* member(const json_value::object& object, std::string_view name) {
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &found->second;
}

bool only_members(const json_value::object& object,
                  std::initializer_list<std::string_view> allowed) {
  return std::ranges::all_of(object, [&](const auto& item) {
    return std::ranges::find(allowed, std::string_view{item.first}) != allowed.end();
  });
}

bool identifier(std::string_view value) {
  if (value.empty() || value.size() > 128)
    return false;
  const auto first = static_cast<unsigned char>(value.front());
  if (!(value.front() == '_' || (first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')))
    return false;
  return std::ranges::all_of(value.substr(1), [](char character) {
    const auto byte = static_cast<unsigned char>(character);
    return character == '_' || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
           (byte >= '0' && byte <= '9');
  });
}

bool logical_name(std::string_view value) {
  if (value.empty() || value.size() > 256 || value.front() == '.' || value.back() == '.')
    return false;
  std::size_t begin = 0;
  while (begin < value.size()) {
    const auto end = value.find('.', begin);
    const auto segment =
        value.substr(begin, end == std::string_view::npos ? value.size() - begin : end - begin);
    if (!identifier(segment))
      return false;
    if (end == std::string_view::npos)
      return true;
    begin = end + 1;
  }
  return true;
}

bool source_path(std::string_view value) {
  if (value.empty() || value.size() > 1024 || value.front() == '/' ||
      value.find('\\') != std::string_view::npos || value.find(':') != std::string_view::npos ||
      !value.ends_with(".hlsl"))
    return false;
  std::size_t begin = 0;
  while (begin <= value.size()) {
    const auto end = value.find('/', begin);
    const auto segment =
        value.substr(begin, end == std::string_view::npos ? value.size() - begin : end - begin);
    if (segment.empty() || segment == "." || segment == "..")
      return false;
    if (end == std::string_view::npos)
      return true;
    begin = end + 1;
  }
  return false;
}

bool parse_stage(std::string_view value, shader_stage& stage) {
  if (value == "vertex")
    stage = shader_stage::vertex;
  else if (value == "fragment")
    stage = shader_stage::fragment;
  else if (value == "compute")
    stage = shader_stage::compute;
  else
    return false;
  return true;
}

bool parse_defines(const json_value::object& object,
                   std::vector<shader_library_source_define>& defines) {
  if (object.empty() || object.size() > maximum_define_count)
    return false;
  defines.reserve(object.size());
  for (const auto& [name, value] : object) {
    const auto* text = as<std::string>(&value);
    if (!identifier(name) || text == nullptr || text->empty() || text->size() > 256)
      return false;
    defines.push_back({name, *text});
  }
  return true;
}

bool parse_variants(const json_value::array& array,
                    std::vector<shader_library_source_variant>& variants) {
  if (array.empty() || array.size() > maximum_variant_count)
    return false;
  std::set<std::string, std::less<>> names;
  std::set<std::string, std::less<>> define_sets;
  variants.reserve(array.size());
  for (const auto& value : array) {
    const auto* object = as<json_value::object>(&value);
    if (object == nullptr || !only_members(*object, {"name", "defines"}))
      return false;
    const auto* name = as<std::string>(member(*object, "name"));
    const auto* defines = as<json_value::object>(member(*object, "defines"));
    shader_library_source_variant variant;
    if (name == nullptr || !identifier(*name) || defines == nullptr ||
        !names.emplace(*name).second || !parse_defines(*defines, variant.defines))
      return false;
    variant.name = *name;
    std::string identity;
    for (const auto& define : variant.defines) {
      identity.append(define.name).push_back('=');
      identity.append(define.value).push_back('\0');
    }
    if (!define_sets.emplace(std::move(identity)).second)
      return false;
    variants.push_back(std::move(variant));
  }
  return true;
}

bool parse_shader(const json_value& value, shader_library_source_shader& shader) {
  const auto* object = as<json_value::object>(&value);
  if (object == nullptr ||
      !only_members(*object, {"name", "source", "stage", "entry_point", "variants"}))
    return false;
  const auto* name = as<std::string>(member(*object, "name"));
  const auto* source = as<std::string>(member(*object, "source"));
  const auto* stage = as<std::string>(member(*object, "stage"));
  const auto* entry_point = as<std::string>(member(*object, "entry_point"));
  if (name == nullptr || !logical_name(*name) || source == nullptr || !source_path(*source) ||
      stage == nullptr || entry_point == nullptr || !identifier(*entry_point) ||
      !parse_stage(*stage, shader.stage))
    return false;
  shader.name = *name;
  shader.source = *source;
  shader.entry_point = *entry_point;
  if (const auto* variants = member(*object, "variants")) {
    const auto* array = as<json_value::array>(variants);
    if (array == nullptr || !parse_variants(*array, shader.variants))
      return false;
  }
  return true;
}

} // namespace

shader_library_source_error
parse_shader_library_source_manifest(std::string_view json,
                                     shader_library_source_manifest& manifest) noexcept {
  if (json.empty() || json.size() > maximum_json_size)
    return shader_library_source_error::invalid_argument;
  try {
    json_value root;
    if (!json_parser{json}.parse(root))
      return shader_library_source_error::invalid_json;
    const auto* object = as<json_value::object>(&root);
    if (object == nullptr || !only_members(*object, {"format_version", "name", "target_profile",
                                                     "target_backends", "shaders"}))
      return shader_library_source_error::invalid_schema;
    const auto* version = as<std::uint64_t>(member(*object, "format_version"));
    if (version == nullptr)
      return shader_library_source_error::invalid_schema;
    if (*version != 1)
      return shader_library_source_error::unsupported_version;
    const auto* name = as<std::string>(member(*object, "name"));
    const auto* profile = as<std::string>(member(*object, "target_profile"));
    const auto* backends = as<json_value::array>(member(*object, "target_backends"));
    const auto* shaders = as<json_value::array>(member(*object, "shaders"));
    if (name == nullptr || !identifier(*name) || profile == nullptr || *profile != "portable" ||
        backends == nullptr || backends->empty() || backends->size() > 2 || shaders == nullptr ||
        shaders->empty() || shaders->size() > maximum_shader_count)
      return shader_library_source_error::invalid_schema;

    shader_library_source_manifest replacement;
    replacement.name = *name;
    std::uint32_t backend_bits = 0;
    for (const auto& backend : *backends) {
      const auto* text = as<std::string>(&backend);
      std::uint32_t bit = 0;
      if (text != nullptr && *text == "vulkan")
        bit = GRANIT_SHADER_BACKEND_VULKAN_BIT;
      else if (text != nullptr && *text == "webgpu")
        bit = GRANIT_SHADER_BACKEND_WEBGPU_BIT;
      else
        return shader_library_source_error::invalid_schema;
      if ((backend_bits & bit) != 0)
        return shader_library_source_error::invalid_schema;
      backend_bits |= bit;
    }
    replacement.target_backends = static_cast<shader_backend>(backend_bits);
    std::set<std::string, std::less<>> names;
    replacement.shaders.reserve(shaders->size());
    for (const auto& value : *shaders) {
      shader_library_source_shader shader;
      if (!parse_shader(value, shader) || !names.emplace(shader.name).second)
        return shader_library_source_error::invalid_schema;
      replacement.shaders.push_back(std::move(shader));
    }
    manifest = std::move(replacement);
    return shader_library_source_error::none;
  } catch (const std::bad_alloc&) {
    return shader_library_source_error::out_of_memory;
  } catch (...) {
    return shader_library_source_error::internal;
  }
}

} // namespace granit::asset_tools::detail
