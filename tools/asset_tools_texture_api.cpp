// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/texture_builder.h>

#include "asset_formats/texture_asset.h"
#include "core/sha256.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct stored_texture_result {
  std::vector<std::byte> manifest;
  std::vector<std::byte> payload;
  std::string debug_json;
  std::string diagnostic;
};

std::mutex texture_results_mutex;
std::unordered_map<uint64_t, std::shared_ptr<const stored_texture_result>> texture_results;
std::atomic<uint64_t> next_texture_result{1};

bool valid_bytes(const void* data, uint64_t size) {
  return (data != nullptr || size == 0) &&
         size <= static_cast<uint64_t>((std::numeric_limits<std::size_t>::max)());
}

granit_asset_tools_texture_result
store_texture_result(std::shared_ptr<const stored_texture_result> value) {
  auto handle = next_texture_result.fetch_add(1, std::memory_order_relaxed);
  if (handle == 0)
    handle = next_texture_result.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard lock{texture_results_mutex};
  texture_results.emplace(handle, std::move(value));
  return handle;
}

std::shared_ptr<const stored_texture_result>
find_texture_result(granit_asset_tools_texture_result handle) {
  std::lock_guard lock{texture_results_mutex};
  const auto iterator = texture_results.find(handle);
  return iterator == texture_results.end() ? nullptr : iterator->second;
}

granit_result fail_with_result(std::shared_ptr<stored_texture_result> value, granit_result status,
                               std::string diagnostic, granit_asset_tools_texture_result* result) {
  value->diagnostic = std::move(diagnostic);
  *result = store_texture_result(std::move(value));
  return status;
}

void append_u32(std::vector<std::byte>& bytes, uint32_t value) {
  for (uint32_t index = 0; index < 4; ++index)
    bytes.push_back(static_cast<std::byte>(value >> (index * 8U)));
}

void append_u64(std::vector<std::byte>& bytes, uint64_t value) {
  for (uint32_t index = 0; index < 8; ++index)
    bytes.push_back(static_cast<std::byte>(value >> (index * 8U)));
}

granit::asset_content_id calculate_content_id(const granit::detail::texture_asset_view& asset) {
  constexpr char domain[] = "granit.texture.asset.v1";
  std::vector<std::byte> identity;
  identity.insert(identity.end(), reinterpret_cast<const std::byte*>(domain),
                  reinterpret_cast<const std::byte*>(domain + sizeof(domain) - 1));
  append_u32(identity, asset.dimension);
  append_u32(identity, asset.width);
  append_u32(identity, asset.height);
  append_u32(identity, asset.depth);
  append_u32(identity, asset.array_layers);
  append_u32(identity, asset.mip_levels);
  append_u32(identity, static_cast<uint32_t>(asset.variants.size()));
  append_u32(identity, static_cast<uint32_t>(asset.subresources.size()));
  for (const auto& variant : asset.variants) {
    append_u32(identity, variant.format);
    append_u32(identity, variant.usage);
    append_u32(identity, variant.first_subresource);
    append_u32(identity, variant.subresource_count);
    append_u64(identity, variant.payload_size);
    identity.insert(identity.end(), reinterpret_cast<const std::byte*>(variant.payload_digest),
                    reinterpret_cast<const std::byte*>(variant.payload_digest) +
                        GRANIT_CONTENT_DIGEST_SIZE);
  }
  for (const auto& subresource : asset.subresources) {
    append_u32(identity, subresource.mip_level);
    append_u32(identity, subresource.array_layer);
    append_u64(identity, subresource.data_offset);
    append_u64(identity, subresource.data_size);
    append_u32(identity, subresource.bytes_per_row);
    append_u32(identity, subresource.rows_per_image);
  }
  return granit::detail::sha256_bytes(identity);
}

std::string make_debug_json(const granit::detail::texture_asset_view& asset) {
  std::ostringstream output;
  output << "{\n  \"magic\": \"GRNTEXA\",\n  \"schema_version\": 1,\n  \"content_id\": \""
         << std::hex << std::setfill('0');
  for (const auto byte : asset.content_id)
    output << std::setw(2) << std::to_integer<unsigned int>(byte);
  output << std::dec << "\",\n  \"dimension\": " << asset.dimension
         << ",\n  \"width\": " << asset.width << ",\n  \"height\": " << asset.height
         << ",\n  \"depth\": " << asset.depth << ",\n  \"array_layers\": " << asset.array_layers
         << ",\n  \"mip_levels\": " << asset.mip_levels << ",\n  \"variants\": [\n";
  for (std::size_t index = 0; index < asset.variants.size(); ++index) {
    const auto& variant = asset.variants[index];
    output << "    {\"format\": " << variant.format << ", \"usage\": " << variant.usage
           << ", \"payload_offset\": " << variant.payload_offset
           << ", \"payload_size\": " << variant.payload_size
           << ", \"subresource_count\": " << variant.subresource_count << "}"
           << (index + 1 == asset.variants.size() ? "\n" : ",\n");
  }
  output << "  ],\n  \"subresources\": [\n";
  for (std::size_t index = 0; index < asset.subresources.size(); ++index) {
    const auto& subresource = asset.subresources[index];
    output << "    {\"mip_level\": " << subresource.mip_level
           << ", \"array_layer\": " << subresource.array_layer
           << ", \"data_offset\": " << subresource.data_offset
           << ", \"data_size\": " << subresource.data_size
           << ", \"bytes_per_row\": " << subresource.bytes_per_row
           << ", \"rows_per_image\": " << subresource.rows_per_image << "}"
           << (index + 1 == asset.subresources.size() ? "\n" : ",\n");
  }
  output << "  ]\n}\n";
  return output.str();
}

granit_result decode_manifest(const void* data, uint64_t size,
                              granit::detail::texture_asset_view& asset) {
  if (!valid_bytes(data, size) || size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto decoded = granit::detail::decode_texture_asset(
      {static_cast<const std::byte*>(data), static_cast<std::size_t>(size)}, asset);
  if (decoded == granit::detail::texture_asset_error::unsupported_schema)
    return GRANIT_ERROR_UNSUPPORTED;
  return decoded == granit::detail::texture_asset_error::success ? GRANIT_SUCCESS
                                                                 : GRANIT_ERROR_INVALID_ARGUMENT;
}

} // namespace

extern "C" {

granit_result granit_asset_tools_texture_build(const granit_asset_tools_texture_build_desc* desc,
                                               granit_asset_tools_texture_result* result) {
  if (result == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *result = 0;
  if (desc == nullptr || desc->struct_size < sizeof(*desc) || desc->reserved[0] != 0 ||
      desc->reserved[1] != 0 || desc->variants == nullptr || desc->variant_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto value = std::make_shared<stored_texture_result>();
    granit::detail::texture_asset_view asset;
    asset.dimension = desc->dimension;
    asset.width = desc->width;
    asset.height = desc->height;
    asset.depth = desc->depth;
    asset.array_layers = desc->array_layers;
    asset.mip_levels = desc->mip_levels;
    uint64_t payload_offset = 0;
    for (uint32_t index = 0; index < desc->variant_count; ++index) {
      const auto& source = desc->variants[index];
      if (source.struct_size < sizeof(source) || source.reserved != 0 || source.reserved2 != 0 ||
          source.format == GRANIT_TEXTURE_FORMAT_UNDEFINED || source.usage == 0 ||
          !valid_bytes(source.payload, source.payload_size) || source.payload_size == 0 ||
          source.payload_size > UINT64_MAX - payload_offset || source.subresources == nullptr ||
          source.subresource_count == 0 ||
          source.subresource_count > UINT32_MAX - asset.subresources.size())
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto first_subresource = static_cast<uint32_t>(asset.subresources.size());
      asset.subresources.insert(asset.subresources.end(), source.subresources,
                                source.subresources + source.subresource_count);
      granit_texture_asset_variant_info variant{};
      variant.format = source.format;
      variant.usage = source.usage;
      variant.first_subresource = first_subresource;
      variant.subresource_count = source.subresource_count;
      variant.payload_offset = payload_offset;
      variant.payload_size = source.payload_size;
      const auto bytes = std::span{static_cast<const std::byte*>(source.payload),
                                   static_cast<std::size_t>(source.payload_size)};
      const auto digest = granit::detail::sha256_bytes(bytes);
      std::memcpy(variant.payload_digest, digest.data(), digest.size());
      asset.variants.push_back(variant);
      value->payload.insert(value->payload.end(), bytes.begin(), bytes.end());
      payload_offset += source.payload_size;
    }
    asset.content_id = calculate_content_id(asset);
    const auto encoded = granit::detail::encode_texture_asset(asset, value->manifest);
    if (encoded != granit::detail::texture_asset_error::success)
      return fail_with_result(std::move(value), GRANIT_ERROR_INVALID_ARGUMENT,
                              "Texture Asset 元数据或子资源布局无效\n", result);
    value->debug_json = make_debug_json(asset);
    *result = store_texture_result(std::move(value));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result granit_asset_tools_texture_inspect(const void* manifest, uint64_t manifest_size,
                                                 granit_asset_tools_texture_result* result) {
  if (result == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *result = 0;
  try {
    auto value = std::make_shared<stored_texture_result>();
    granit::detail::texture_asset_view asset;
    const auto status = decode_manifest(manifest, manifest_size, asset);
    if (status != GRANIT_SUCCESS)
      return fail_with_result(std::move(value), status,
                              "Texture Asset Manifest 无效或版本不受支持\n", result);
    const auto bytes =
        std::span{static_cast<const std::byte*>(manifest), static_cast<std::size_t>(manifest_size)};
    value->manifest.assign(bytes.begin(), bytes.end());
    value->debug_json = make_debug_json(asset);
    *result = store_texture_result(std::move(value));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
granit_asset_tools_texture_result_get_manifest(granit_asset_tools_texture_result result,
                                               const void** data, uint64_t* size) {
  if (data == nullptr || size == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *data = nullptr;
  *size = 0;
  const auto value = find_texture_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  *data = value->manifest.data();
  *size = value->manifest.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_texture_result_get_payload(granit_asset_tools_texture_result result,
                                              const void** data, uint64_t* size) {
  if (data == nullptr || size == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *data = nullptr;
  *size = 0;
  const auto value = find_texture_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  *data = value->payload.data();
  *size = value->payload.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_texture_result_get_debug_json(granit_asset_tools_texture_result result,
                                                 const char** json, uint64_t* length) {
  if (json == nullptr || length == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *json = nullptr;
  *length = 0;
  const auto value = find_texture_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  *json = value->debug_json.data();
  *length = value->debug_json.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_texture_result_get_diagnostic(granit_asset_tools_texture_result result,
                                                 const char** diagnostic, uint64_t* length) {
  if (diagnostic == nullptr || length == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *diagnostic = nullptr;
  *length = 0;
  const auto value = find_texture_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  *diagnostic = value->diagnostic.data();
  *length = value->diagnostic.size();
  return GRANIT_SUCCESS;
}

granit_result granit_asset_tools_texture_result_destroy(granit_asset_tools_texture_result result) {
  std::lock_guard lock{texture_results_mutex};
  return texture_results.erase(result) == 1 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_HANDLE;
}

} // extern "C"
