// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "object_cache.h"

#include "../shader_object_storage.h"
#include "../shader_tools_core.h"
#include "shader_format/shader_cache_key.h"
#include "shader_format/shader_object.h"

#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace granit::tools {
namespace {

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

bool write_bytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
  std::error_code error;
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path(), error);
  if (error)
    return false;
  std::ofstream stream{path, std::ios::binary | std::ios::trunc};
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(stream);
}

std::string_view stage_name(shader_stage stage) {
  switch (stage) {
  case shader_stage::vertex:
    return "vertex";
  case shader_stage::fragment:
    return "fragment";
  case shader_stage::compute:
    return "compute";
  }
  return {};
}

bool valid_context(const shader_object_cache_context& context) {
  return !context.source_path.empty() && !context.wgsl_path.empty() &&
         !context.spirv_path.empty() && !context.object_path.empty() &&
         !context.entry_point.empty() && !context.tool_identity.empty() &&
         !context.target_environment.empty() && !stage_name(context.stage).empty() &&
         context.backend_mask != 0 &&
         (context.backend_mask & ~GRANIT_SHADER_BACKEND_ALL_BITS) == 0 &&
         (context.required_features & ~GRANIT_SHADER_FEATURE_ALL_BITS) == 0;
}

} // namespace

granit_result restore_shader_object_cache(const shader_object_cache_context& context,
                                          bool& cache_hit) noexcept {
  cache_hit = false;
  if (!valid_context(context))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    const auto source = read_text(context.source_path);
    if (source.empty())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    detail::shader_format::shader_object_view object;
    if (detail::shader_format::decode_shader_object(read_bytes(context.object_path), object) !=
        detail::shader_format::shader_object_error::success)
      return GRANIT_SUCCESS;
    std::uint32_t packaged_backends = 0;
    if (detail::shader_format::find_shader_object_variant(
            object, detail::shader_format::shader_object_backend::vulkan,
            shader_profile::portable) != nullptr)
      packaged_backends |= GRANIT_SHADER_BACKEND_VULKAN_BIT;
    if (detail::shader_format::find_shader_object_variant(
            object, detail::shader_format::shader_object_backend::webgpu,
            shader_profile::portable) != nullptr)
      packaged_backends |= GRANIT_SHADER_BACKEND_WEBGPU_BIT;
    const auto key = detail::shader_format::make_shader_cache_key(
        {source, "hlsl", context.entry_point, stage_name(context.stage), context.tool_identity,
         context.target_environment, context.compile_options, context.required_features});
    if (packaged_backends != context.backend_mask || key != object.cache_key)
      return GRANIT_SUCCESS;

    auto wgsl_sidecar = context.object_path;
    wgsl_sidecar += ".wgsl";
    auto spirv_sidecar = context.object_path;
    spirv_sidecar += ".spv";
    const auto wgsl = read_text(wgsl_sidecar);
    const auto spirv = read_bytes(spirv_sidecar);
    if (detail::shader_format::validate_shader_object_payloads(object, wgsl, spirv) !=
        detail::shader_format::shader_object_error::success)
      return GRANIT_SUCCESS;
    if (!write_bytes(context.spirv_path, spirv) ||
        !write_bytes(context.wgsl_path, std::as_bytes(std::span{wgsl})))
      return GRANIT_ERROR_INITIALIZATION_FAILED;
    cache_hit = true;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result write_shader_object_cache(const shader_object_cache_context& context,
                                        bool& cache_hit) noexcept {
  cache_hit = false;
  if (!valid_context(context) || context.required_features != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    const auto source = read_text(context.source_path);
    const auto wgsl = read_text(context.wgsl_path);
    const auto spirv = read_bytes(context.spirv_path);
    shader_info info;
    std::ostringstream output;
    std::ostringstream diagnostic;
    if (source.empty() || wgsl.empty() || spirv.empty() ||
        !inspect_shader(context.spirv_path, false, info, output, diagnostic) ||
        info.entry_point != context.entry_point || info.stage != stage_name(context.stage))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto key = detail::shader_format::make_shader_cache_key(
        {source, "hlsl", context.entry_point, stage_name(context.stage), context.tool_identity,
         context.target_environment, context.compile_options, context.required_features});
    std::vector<std::byte> object;
    if (detail::shader_format::encode_shader_object(
            {wgsl, spirv, serialize_shader_info_json(info), key, context.backend_mask,
             context.required_features, context.stage, context.entry_point},
            object) != detail::shader_format::shader_object_error::success)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (granit::tools::store_shader_object(context.object_path, object, wgsl, spirv, cache_hit) !=
        detail::shader_format::shader_object_error::success)
      return GRANIT_ERROR_INITIALIZATION_FAILED;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

} // namespace granit::tools
