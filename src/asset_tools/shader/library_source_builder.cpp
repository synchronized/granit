// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/shader_compiler.h>
#include <granit/asset_tools/shader_library_builder.h>

#include "asset_formats/shader/shader_library.h"
#include "asset_formats/shader/shader_object.h"
#include "asset_tools/common/toolchain_layout.h"
#include "asset_tools/shader/library_builder.h"
#include "asset_tools/shader/library_source_manifest.h"
#include "asset_tools/shader/object_cache.h"
#include "asset_tools/shader/object_storage.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool valid_string(const char* value, std::uint64_t length) {
  return (value != nullptr || length == 0) &&
         length <= static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
}

std::filesystem::path copy_path(const char* value, std::uint64_t length) {
  std::u8string text(static_cast<std::size_t>(length), u8'\0');
  if (length != 0)
    std::memcpy(text.data(), value, static_cast<std::size_t>(length));
  return std::filesystem::path{text};
}

std::filesystem::path copy_path(std::string_view value) {
  std::u8string text(value.size(), u8'\0');
  if (!value.empty())
    std::memcpy(text.data(), value.data(), value.size());
  return std::filesystem::path{text};
}

std::string path_text(const std::filesystem::path& path) {
  const auto text = path.generic_u8string();
  return {reinterpret_cast<const char*>(text.data()), text.size()};
}

std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary | std::ios::ate};
  if (!stream)
    return {};
  const auto size = stream.tellg();
  if (size <= 0)
    return {};
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), size);
  return stream ? bytes : std::vector<std::byte>{};
}

std::string read_text(const std::filesystem::path& path) {
  const auto bytes = read_bytes(path);
  if (bytes.empty())
    return {};
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

bool write_text_if_changed(const std::filesystem::path& path, std::string_view text,
                           bool& cache_hit) {
  const auto current = read_bytes(path);
  const auto bytes = std::as_bytes(std::span{text});
  if (std::ranges::equal(current, bytes)) {
    cache_hit = true;
    return true;
  }
  cache_hit = false;
  std::error_code error;
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path(), error);
  if (error)
    return false;
  std::ofstream stream{path, std::ios::binary | std::ios::trunc};
  stream.write(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(stream);
}

granit_result source_error(granit::asset_tools::detail::shader_library_source_error error) {
  using enum granit::asset_tools::detail::shader_library_source_error;
  switch (error) {
  case none:
    return GRANIT_SUCCESS;
  case out_of_memory:
    return GRANIT_ERROR_OUT_OF_MEMORY;
  case internal:
    return GRANIT_ERROR_INTERNAL;
  case invalid_argument:
  case invalid_json:
  case invalid_schema:
  case unsupported_version:
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_ERROR_INTERNAL;
}

std::string tool_identity(const std::filesystem::path& path) {
  return granit::asset_tools::detail::file_sha256_hex(path);
}

std::string object_stem(std::string_view name) {
  constexpr std::string_view digits = "0123456789abcdef";
  std::string result;
  result.reserve(name.size() * 2);
  for (const auto character : name) {
    const auto value = static_cast<unsigned char>(character);
    result.push_back(digits[value >> 4]);
    result.push_back(digits[value & 0xf]);
  }
  return result;
}

std::string compile_options(
    const std::vector<granit::asset_tools::detail::shader_library_source_define>& definitions) {
  std::string result = "source=hlsl;spirv=vulkan1.3;bridge=spirv1.3";
  for (const auto& define : definitions) {
    result += ";define=" + std::to_string(define.name.size()) + ":" + define.name + ":" +
              std::to_string(define.value.size()) + ":" + define.value;
  }
  return result;
}

struct compiler_owner {
  granit_asset_tools_shader_compiler value{};
  ~compiler_owner() {
    if (value != 0)
      static_cast<void>(granit_asset_tools_shader_compiler_destroy(value));
  }
};

struct compilation_owner {
  granit_asset_tools_shader_compilation value{};
  ~compilation_owner() {
    if (value != 0)
      static_cast<void>(granit_asset_tools_shader_compilation_destroy(value));
  }
};

struct expanded_shader {
  std::string name;
  const granit::asset_tools::detail::shader_library_source_shader* shader{};
  const std::vector<granit::asset_tools::detail::shader_library_source_define>* definitions{};
};

constexpr std::size_t maximum_expanded_shader_count = 4096;

} // namespace

extern "C" granit_result granit_asset_tools_shader_build_library_from_manifest(
    const granit_asset_tools_shader_source_library_desc* desc, std::uint32_t* cache_hit) {
  if (cache_hit == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *cache_hit = 0;
  if (desc == nullptr || desc->struct_size < sizeof(*desc) || desc->reserved != 0 ||
      !valid_string(desc->manifest_path, desc->manifest_path_length) ||
      !valid_string(desc->toolchain_root, desc->toolchain_root_length) ||
      !valid_string(desc->cache_path, desc->cache_path_length) ||
      !valid_string(desc->output_path, desc->output_path_length) ||
      !valid_string(desc->index_path, desc->index_path_length) || desc->manifest_path_length == 0 ||
      desc->toolchain_root_length == 0 || desc->cache_path_length == 0 ||
      desc->output_path_length == 0 || desc->index_path_length == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  try {
    const auto manifest_path = copy_path(desc->manifest_path, desc->manifest_path_length);
    granit::asset_tools::detail::shader_library_source_manifest manifest;
    const auto parse_result = granit::asset_tools::detail::parse_shader_library_source_manifest(
        read_text(manifest_path), manifest);
    if (parse_result != granit::asset_tools::detail::shader_library_source_error::none)
      return source_error(parse_result);

    const auto cache_path = copy_path(desc->cache_path, desc->cache_path_length);
    std::error_code filesystem_error;
    std::filesystem::create_directories(cache_path, filesystem_error);
    if (filesystem_error)
      return GRANIT_ERROR_INITIALIZATION_FAILED;

    const auto toolchain_root = copy_path(desc->toolchain_root, desc->toolchain_root_length);
    const auto toolchain = granit::asset_tools::detail::resolve_shader_toolchain(toolchain_root);
    if (!granit::asset_tools::detail::shader_toolchain_ready(toolchain))
      return GRANIT_ERROR_NOT_READY;
    const auto dxc_identity = tool_identity(toolchain.dxc);
    const auto tint_identity = tool_identity(toolchain.tint);
    const auto revisions = "dxc=" + dxc_identity + ";tint=" + tint_identity;
    constexpr std::string_view target_environment = "vulkan1.3+webgpu-portable";

    compiler_owner compiler;
    const granit_asset_tools_shader_compiler_desc compiler_desc{
        .struct_size = sizeof(granit_asset_tools_shader_compiler_desc),
        .reserved = 0,
        .toolchain_root = desc->toolchain_root,
        .toolchain_root_length = desc->toolchain_root_length,
    };
    auto status = granit_asset_tools_shader_compiler_create(&compiler_desc, &compiler.value);
    if (status != GRANIT_SUCCESS)
      return status;

    std::vector<expanded_shader> expanded;
    for (const auto& shader : manifest.shaders) {
      if (shader.variants.empty()) {
        if (expanded.size() == maximum_expanded_shader_count)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        expanded.push_back({shader.name, &shader, nullptr});
      } else {
        for (const auto& variant : shader.variants) {
          if (expanded.size() == maximum_expanded_shader_count)
            return GRANIT_ERROR_INVALID_ARGUMENT;
          expanded.push_back({shader.name + "/" + variant.name, &shader, &variant.defines});
        }
      }
    }
    std::ranges::sort(expanded, {}, &expanded_shader::name);

    std::vector<std::filesystem::path> object_paths;
    object_paths.reserve(expanded.size());
    granit::asset_tools::detail::shader_library_index index{
        .library = manifest.name, .library_digest = {}, .shaders = {}};
    bool all_objects_hit = true;
    const std::vector<granit::asset_tools::detail::shader_library_source_define> no_defines;
    for (const auto& item : expanded) {
      const auto& definitions = item.definitions == nullptr ? no_defines : *item.definitions;
      const auto source = manifest_path.parent_path() / copy_path(item.shader->source);
      const auto object = cache_path / (object_stem(item.name) + ".grshaderobj");
      auto spirv = object;
      spirv += ".spv";
      auto wgsl = object;
      wgsl += ".wgsl";
      const auto source_string = path_text(source);
      const auto object_string = path_text(object);
      const auto spirv_string = path_text(spirv);
      const auto wgsl_string = path_text(wgsl);
      const auto options = compile_options(definitions);

      const granit::asset_tools::detail::shader_object_cache_context object_context{
          .source_path = source,
          .wgsl_path = wgsl,
          .spirv_path = spirv,
          .object_path = object,
          .entry_point = item.shader->entry_point,
          .stage = item.shader->stage,
          .tool_identity = revisions,
          .target_environment = target_environment,
          .compile_options = options,
          .backend_mask = static_cast<granit_shader_backend_flags>(manifest.target_backends),
          .required_features = 0,
      };
      bool object_hit = false;
      status = granit::asset_tools::detail::restore_shader_object_cache(object_context, object_hit);
      if (status != GRANIT_SUCCESS)
        return status;

      if (!object_hit) {
        std::vector<granit_asset_tools_shader_define> native_defines;
        native_defines.reserve(definitions.size());
        for (const auto& define : definitions) {
          native_defines.push_back({sizeof(granit_asset_tools_shader_define), 0, define.name.data(),
                                    define.name.size(), define.value.data(), define.value.size()});
        }
        const granit_asset_tools_shader_compile_desc compile{
            .struct_size = sizeof(granit_asset_tools_shader_compile_desc),
            .stage = static_cast<granit_shader_stage>(item.shader->stage),
            .target_backends = static_cast<granit_shader_backend_flags>(manifest.target_backends),
            .input_path = source_string.data(),
            .input_path_length = source_string.size(),
            .entry_point = item.shader->entry_point.data(),
            .entry_point_length = item.shader->entry_point.size(),
            .spirv_output_path = spirv_string.data(),
            .spirv_output_path_length = spirv_string.size(),
            .wgsl_output_path = wgsl_string.data(),
            .wgsl_output_path_length = wgsl_string.size(),
            .defines = native_defines.data(),
            .define_count = static_cast<std::uint32_t>(native_defines.size()),
            .validate_binding_set = 0,
            .expected_bindings = nullptr,
            .expected_binding_count = 0,
        };
        compilation_owner compilation;
        status = granit_asset_tools_shader_compiler_compile(compiler.value, &compile,
                                                            &compilation.value);
        if (status != GRANIT_SUCCESS)
          return status;
        status = granit::asset_tools::detail::write_shader_object_cache(object_context, object_hit);
        if (status != GRANIT_SUCCESS)
          return status;
        all_objects_hit = false;
      }

      granit::detail::shader_format::shader_object_view object_view;
      const auto object_bytes = read_bytes(object);
      if (granit::detail::shader_format::decode_shader_object(object_bytes, object_view) !=
          granit::detail::shader_format::shader_object_error::success)
        return GRANIT_ERROR_INTERNAL;
      index.shaders.push_back(
          {item.name, object_view.content_id, item.shader->stage, item.shader->entry_point});
      object_paths.push_back(object);
    }

    bool library_hit = false;
    status = granit::asset_tools::detail::link_shader_library(
        object_paths, static_cast<granit_shader_backend_flags>(manifest.target_backends),
        copy_path(desc->output_path, desc->output_path_length), library_hit);
    if (status != GRANIT_SUCCESS)
      return status;

    granit::detail::shader_format::shader_library_view library;
    if (granit::detail::shader_format::decode_shader_library(
            read_bytes(copy_path(desc->output_path, desc->output_path_length)), library) !=
        granit::detail::shader_format::shader_library_error::success)
      return GRANIT_ERROR_INTERNAL;
    index.library_digest = library.content_digest;
    std::string index_json;
    const auto encode_result =
        granit::asset_tools::detail::encode_shader_library_index_json(index, index_json);
    if (encode_result != granit::asset_tools::detail::shader_library_source_error::none)
      return source_error(encode_result);
    bool index_hit = false;
    if (!write_text_if_changed(copy_path(desc->index_path, desc->index_path_length), index_json,
                               index_hit))
      return GRANIT_ERROR_INITIALIZATION_FAILED;
    *cache_hit = all_objects_hit && library_hit && index_hit ? 1U : 0U;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_asset_tools_shader_index_find_content_id(
    const char* index_json, std::uint64_t index_json_length, const char* logical_name,
    std::uint64_t logical_name_length, granit_shader_content_id content_id) {
  if (content_id == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::memset(content_id, 0, sizeof(granit_shader_content_id));
  if (!valid_string(index_json, index_json_length) || index_json_length == 0 ||
      !valid_string(logical_name, logical_name_length) || logical_name_length == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  try {
    granit::asset_tools::detail::shader_library_index index;
    const std::string_view json{index_json, static_cast<std::size_t>(index_json_length)};
    if (granit::asset_tools::detail::parse_shader_library_index_json(json, index) !=
        granit::asset_tools::detail::shader_library_source_error::none)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const std::string_view name{logical_name, static_cast<std::size_t>(logical_name_length)};
    const auto found = std::ranges::find(
        index.shaders, name, &granit::asset_tools::detail::shader_library_index_entry::name);
    if (found == index.shaders.end())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    std::memcpy(content_id, found->content_id.data(), found->content_id.size());
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
