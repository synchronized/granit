// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/material_builder.h>

#include "asset_formats/material/material_package_archive.h"
#include "asset_formats/shader/shader_library.h"
#include "asset_tools/material/debug_json.h"
#include "asset_tools/material/source_json.h"
#include "core/shared_handle_table.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct stored_material_result {
  std::vector<std::byte> archive;
  std::string debug_json;
  std::string diagnostic;
};

granit::detail::shared_handle_table<stored_material_result,
                                    granit::detail::handle_type::asset_tools_material_result>
    material_results;

bool valid_bytes(const void* data, uint64_t size) {
  return (data != nullptr || size == 0) &&
         size <= static_cast<uint64_t>(std::numeric_limits<std::size_t>::max());
}

granit_asset_tools_material_result
store_material_result(std::shared_ptr<const stored_material_result> value) {
  return material_results.insert(std::move(value));
}

std::shared_ptr<const stored_material_result>
find_material_result(granit_asset_tools_material_result handle) {
  return material_results.find(handle);
}

granit_result fail_with_result(std::shared_ptr<stored_material_result> value,
                               std::string diagnostic, granit_asset_tools_material_result* result) {
  value->diagnostic = std::move(diagnostic);
  *result = store_material_result(std::move(value));
  return GRANIT_ERROR_INVALID_ARGUMENT;
}

} // namespace

extern "C" {

granit_result granit_asset_tools_material_build(const granit_asset_tools_material_build_desc* desc,
                                                granit_asset_tools_material_result* result) {
  if (result == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *result = 0;
  if (desc == nullptr || desc->struct_size < sizeof(*desc) || desc->reserved != 0 ||
      desc->reserved2 != 0 || !valid_bytes(desc->source_json, desc->source_json_length) ||
      desc->source_json_length == 0 || desc->shader_library_count == 0 ||
      desc->shader_library_count > 1024 || desc->shader_libraries == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto value = std::make_shared<stored_material_result>();
    std::vector<granit::material::material_shader_reference> references;
    std::vector<std::string> libraries;
    for (uint32_t index = 0; index < desc->shader_library_count; ++index) {
      const auto& source = desc->shader_libraries[index];
      if (source.struct_size < sizeof(source) || source.reserved != 0 ||
          !valid_bytes(source.archive, source.archive_size) || source.archive_size == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      granit::detail::shader_format::shader_library_view shader_library;
      const auto archive = std::span{static_cast<const std::byte*>(source.archive),
                                     static_cast<std::size_t>(source.archive_size)};
      if (granit::detail::shader_format::decode_shader_library(archive, shader_library) !=
              granit::detail::shader_format::shader_library_error::success ||
          std::ranges::find(libraries, shader_library.name) != libraries.end()) {
        return fail_with_result(std::move(value), "Shader Library 无效或 Library 名称重复\n",
                                result);
      }
      libraries.emplace_back(shader_library.name);
      for (const auto& name : shader_library.names) {
        const auto* shader = granit::detail::shader_format::find_shader_library_shader(
            shader_library, name.content_id);
        if (shader == nullptr)
          return GRANIT_ERROR_INTERNAL;
        if (shader->stage == granit::shader_stage::compute)
          continue;
        const auto stage = shader->stage == granit::shader_stage::vertex
                               ? granit::material::package_shader_stage::vertex
                               : granit::material::package_shader_stage::fragment;
        references.push_back({std::string{shader_library.name}, std::string{name.name},
                              shader->content_id, stage, std::string{shader->entry_point}});
      }
    }

    granit::material::material_package package;
    const std::string_view source_json{desc->source_json,
                                       static_cast<std::size_t>(desc->source_json_length)};
    if (granit::material::parse_material_source_json(source_json, references, package) !=
        granit::material::source_json_error::none) {
      return fail_with_result(std::move(value), "材质源描述、Shader 引用或包语义无效\n", result);
    }
    if (granit::material::encode_material_package_archive(package, value->archive) !=
            granit::material::archive_error::none ||
        granit::material::export_material_archive_debug_json(value->archive, value->debug_json) !=
            granit::material::archive_error::none) {
      return fail_with_result(std::move(value), "材质包编码失败\n", result);
    }
    *result = store_material_result(std::move(value));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result granit_asset_tools_material_inspect(const void* archive, uint64_t archive_size,
                                                  granit_asset_tools_material_result* result) {
  if (result == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *result = 0;
  if (!valid_bytes(archive, archive_size) || archive_size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto value = std::make_shared<stored_material_result>();
    const auto bytes =
        std::span{static_cast<const std::byte*>(archive), static_cast<std::size_t>(archive_size)};
    if (granit::material::export_material_archive_debug_json(bytes, value->debug_json) !=
        granit::material::archive_error::none) {
      return fail_with_result(std::move(value), "材质包验证或解码失败\n", result);
    }
    value->archive.assign(bytes.begin(), bytes.end());
    *result = store_material_result(std::move(value));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
granit_asset_tools_material_result_get_info(granit_asset_tools_material_result result,
                                            granit_asset_tools_material_result_info* info) {
  if (info == nullptr || info->struct_size < sizeof(*info) || info->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto value = find_material_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  info->archive = value->archive.data();
  info->archive_size = value->archive.size();
  info->debug_json = value->debug_json.data();
  info->debug_json_length = value->debug_json.size();
  info->diagnostic = value->diagnostic.data();
  info->diagnostic_length = value->diagnostic.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_material_result_destroy(granit_asset_tools_material_result result) {
  return material_results.erase(result);
}

} // extern "C"
