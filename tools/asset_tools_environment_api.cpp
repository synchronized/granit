// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/environment_builder.h>

#include "assets/environment_asset.h"

#include <atomic>
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

struct stored_environment_result {
  std::vector<std::byte> package;
  std::string debug_json;
  std::string diagnostic;
};

std::mutex environment_results_mutex;
std::unordered_map<uint64_t, std::shared_ptr<const stored_environment_result>> environment_results;
std::atomic<uint64_t> next_environment_result{1};

bool valid_bytes(const void* data, uint64_t size) {
  return (data != nullptr || size == 0) &&
         size <= static_cast<uint64_t>((std::numeric_limits<std::size_t>::max)());
}

granit_asset_tools_environment_result
store_environment_result(std::shared_ptr<const stored_environment_result> value) {
  auto handle = next_environment_result.fetch_add(1, std::memory_order_relaxed);
  if (handle == 0)
    handle = next_environment_result.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard lock{environment_results_mutex};
  environment_results.emplace(handle, std::move(value));
  return handle;
}

std::shared_ptr<const stored_environment_result>
find_environment_result(granit_asset_tools_environment_result handle) {
  std::lock_guard lock{environment_results_mutex};
  const auto iterator = environment_results.find(handle);
  return iterator == environment_results.end() ? nullptr : iterator->second;
}

granit_result fail_with_result(std::shared_ptr<stored_environment_result> value,
                               granit_result status, std::string diagnostic,
                               granit_asset_tools_environment_result* result) {
  value->diagnostic = std::move(diagnostic);
  *result = store_environment_result(std::move(value));
  return status;
}

std::string make_debug_json(const granit::detail::environment_package& package) {
  auto byte_size = package.irradiance_pixels.size() + package.brdf_pixels.size();
  for (const auto& mip : package.prefiltered_mips)
    byte_size += mip.pixels.size();
  std::ostringstream output;
  output << std::setprecision(9) << "{\n  \"magic\": \"GRENV03\",\n  \"schema_version\": 3,\n"
         << "  \"pixel_format\": \"rgba16-float\",\n"
         << "  \"recommended_environment_intensity\": " << package.recommended_environment_intensity
         << ",\n"
         << "  \"recommended_exposure_ev\": " << package.recommended_exposure_ev << ",\n"
         << "  \"irradiance_resolution\": " << package.irradiance_resolution << ",\n"
         << "  \"prefiltered_mips\": [\n";
  for (std::size_t index = 0; index < package.prefiltered_mips.size(); ++index) {
    const auto& mip = package.prefiltered_mips[index];
    output << "    {\"resolution\": " << mip.resolution << ", \"byte_size\": " << mip.pixels.size()
           << "}" << (index + 1 == package.prefiltered_mips.size() ? "\n" : ",\n");
  }
  output << "  ],\n  \"brdf_width\": " << package.brdf_width
         << ",\n  \"brdf_height\": " << package.brdf_height << ",\n  \"byte_size\": " << byte_size
         << "\n}\n";
  return output.str();
}

granit_result map_error(granit::detail::environment_package_error error) {
  if (error == granit::detail::environment_package_error::none)
    return GRANIT_SUCCESS;
  if (error == granit::detail::environment_package_error::unsupported_version)
    return GRANIT_ERROR_UNSUPPORTED;
  return GRANIT_ERROR_INVALID_ARGUMENT;
}

} // namespace

extern "C" {

granit_result
granit_asset_tools_environment_build(const granit_asset_tools_environment_build_desc* desc,
                                     granit_asset_tools_environment_result* result) {
  if (result == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *result = 0;
  if (desc == nullptr || desc->struct_size < sizeof(*desc) || desc->reserved[0] != 0 ||
      desc->reserved[1] != 0 ||
      !valid_bytes(desc->irradiance_pixels, desc->irradiance_pixels_size) ||
      desc->prefiltered_mips == nullptr || desc->prefiltered_mip_count == 0 ||
      !valid_bytes(desc->brdf_pixels, desc->brdf_pixels_size)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    auto value = std::make_shared<stored_environment_result>();
    granit::detail::environment_package package;
    package.recommended_environment_intensity = desc->recommended_environment_intensity;
    package.recommended_exposure_ev = desc->recommended_exposure_ev;
    package.irradiance_resolution = desc->irradiance_resolution;
    package.irradiance_pixels = {static_cast<const std::byte*>(desc->irradiance_pixels),
                                 static_cast<std::size_t>(desc->irradiance_pixels_size)};
    package.prefiltered_mips.reserve(desc->prefiltered_mip_count);
    for (uint32_t index = 0; index < desc->prefiltered_mip_count; ++index) {
      const auto& mip = desc->prefiltered_mips[index];
      if (mip.struct_size < sizeof(mip) || mip.reserved[0] != 0 || mip.reserved[1] != 0 ||
          !valid_bytes(mip.pixels, mip.pixels_size)) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
      package.prefiltered_mips.push_back(
          {mip.resolution,
           {static_cast<const std::byte*>(mip.pixels), static_cast<std::size_t>(mip.pixels_size)}});
    }
    package.brdf_width = desc->brdf_width;
    package.brdf_height = desc->brdf_height;
    package.brdf_pixels = {static_cast<const std::byte*>(desc->brdf_pixels),
                           static_cast<std::size_t>(desc->brdf_pixels_size)};
    const auto encoded = granit::detail::encode_environment_package(package, value->package);
    if (encoded != granit::detail::environment_package_error::none) {
      return fail_with_result(std::move(value), map_error(encoded),
                              "Environment Asset 像素尺寸、mip 链或推荐参数无效\n", result);
    }
    value->debug_json = make_debug_json(package);
    *result = store_environment_result(std::move(value));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
granit_asset_tools_environment_inspect(const void* package_data, uint64_t package_size,
                                       granit_asset_tools_environment_result* result) {
  if (result == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *result = 0;
  if (!valid_bytes(package_data, package_size) || package_size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    auto value = std::make_shared<stored_environment_result>();
    const auto bytes = std::span{static_cast<const std::byte*>(package_data),
                                 static_cast<std::size_t>(package_size)};
    granit::detail::environment_package package;
    const auto decoded = granit::detail::parse_environment_package(bytes, package);
    if (decoded != granit::detail::environment_package_error::none) {
      return fail_with_result(std::move(value), map_error(decoded),
                              "Environment Asset 无效、摘要不匹配或版本不受支持\n", result);
    }
    value->package.assign(bytes.begin(), bytes.end());
    value->debug_json = make_debug_json(package);
    *result = store_environment_result(std::move(value));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
granit_asset_tools_environment_result_get_package(granit_asset_tools_environment_result result,
                                                  const void** data, uint64_t* size) {
  if (data == nullptr || size == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *data = nullptr;
  *size = 0;
  const auto value = find_environment_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  *data = value->package.data();
  *size = value->package.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_environment_result_get_debug_json(granit_asset_tools_environment_result result,
                                                     const char** json, uint64_t* length) {
  if (json == nullptr || length == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *json = nullptr;
  *length = 0;
  const auto value = find_environment_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  *json = value->debug_json.data();
  *length = value->debug_json.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_environment_result_get_diagnostic(granit_asset_tools_environment_result result,
                                                     const char** diagnostic, uint64_t* length) {
  if (diagnostic == nullptr || length == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *diagnostic = nullptr;
  *length = 0;
  const auto value = find_environment_result(result);
  if (value == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  *diagnostic = value->diagnostic.data();
  *length = value->diagnostic.size();
  return GRANIT_SUCCESS;
}

granit_result
granit_asset_tools_environment_result_destroy(granit_asset_tools_environment_result result) {
  std::lock_guard lock{environment_results_mutex};
  return environment_results.erase(result) == 1 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_HANDLE;
}

} // extern "C"
