// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/material_builder.h>

#include "material/material_debug_json.h"
#include "asset_formats/material/material_package_archive.h"
#include "material/material_source_json.h"
#include "shader_library/source_manifest.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct stored_material_result {
  std::vector<std::byte> archive;
  std::string debug_json;
  std::string diagnostic;
};

std::mutex material_results_mutex;
std::unordered_map<uint64_t, std::shared_ptr<const stored_material_result>> material_results;
std::atomic<uint64_t> next_material_result{1};

bool valid_bytes(const void* data, uint64_t size) {
  return (data != nullptr || size == 0) &&
         size <= static_cast<uint64_t>(std::numeric_limits<std::size_t>::max());
}

granit_asset_tools_material_result
store_material_result(std::shared_ptr<const stored_material_result> value) {
  auto handle = next_material_result.fetch_add(1, std::memory_order_relaxed);
  if (handle == 0)
    handle = next_material_result.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard lock{material_results_mutex};
  material_results.emplace(handle, std::move(value));
  return handle;
}

std::shared_ptr<const stored_material_result>
find_material_result(granit_asset_tools_material_result handle) {
  std::lock_guard lock{material_results_mutex};
  const auto iterator = material_results.find(handle);
  return iterator == material_results.end() ? nullptr : iterator->second;
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
      desc->source_json_length == 0 || desc->shader_index_count == 0 ||
      desc->shader_index_count > 1024 || desc->shader_indices == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto value = std::make_shared<stored_material_result>();
    std::vector<granit::material::material_shader_reference> references;
    std::vector<std::string> libraries;
    for (uint32_t index = 0; index < desc->shader_index_count; ++index) {
      const auto& source = desc->shader_indices[index];
      if (source.struct_size < sizeof(source) || source.reserved != 0 ||
          !valid_bytes(source.json, source.json_length) || source.json_length == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      granit::tools::shader_library_index shader_index;
      const std::string_view json{source.json, static_cast<std::size_t>(source.json_length)};
      if (granit::tools::parse_shader_library_index_json(json, shader_index) !=
              granit::tools::shader_library_source_error::none ||
          std::ranges::find(libraries, shader_index.library) != libraries.end()) {
        return fail_with_result(std::move(value), "Shader Library 索引无效或 Library 名称重复\n",
                                result);
      }
      libraries.push_back(shader_index.library);
      for (const auto& shader : shader_index.shaders) {
        if (shader.stage == granit::shader_stage::compute)
          continue;
        const auto stage = shader.stage == granit::shader_stage::vertex
                               ? granit::material::package_shader_stage::vertex
                               : granit::material::package_shader_stage::fragment;
        references.push_back(
            {shader_index.library, shader.name, shader.content_id, stage, shader.entry_point});
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
granit_asset_tools_material_result_get_archive(granit_asset_tools_material_result result,
                                               const void** data, uint64_t* size) {
  if (data == nullptr || size == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *data = nullptr;
  *size = 0;
  const auto value = find_material_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  *data = value->archive.data();
  *size = value->archive.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_material_result_get_debug_json(granit_asset_tools_material_result result,
                                                  const char** json, uint64_t* length) {
  if (json == nullptr || length == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *json = nullptr;
  *length = 0;
  const auto value = find_material_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  *json = value->debug_json.data();
  *length = value->debug_json.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_material_result_get_diagnostic(granit_asset_tools_material_result result,
                                                  const char** diagnostic, uint64_t* length) {
  if (diagnostic == nullptr || length == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *diagnostic = nullptr;
  *length = 0;
  const auto value = find_material_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  *diagnostic = value->diagnostic.data();
  *length = value->diagnostic.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_material_result_destroy(granit_asset_tools_material_result result) {
  std::lock_guard lock{material_results_mutex};
  return material_results.erase(result) == 1 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_HANDLE;
}

} // extern "C"
