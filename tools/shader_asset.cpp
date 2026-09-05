// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_asset.h"

#include <algorithm>
#include <atomic>
#include <fstream>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

namespace granit::tools {
namespace {

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
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

bool replace_file(const std::filesystem::path& source, const std::filesystem::path& target) {
#if defined(_WIN32)
  return MoveFileExW(source.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code error;
  std::filesystem::rename(source, target, error);
  return !error;
#endif
}

bool write_file_atomically(const std::filesystem::path& path, std::span<const std::byte> bytes) {
  static std::atomic<std::uint64_t> sequence{0};
#if defined(_WIN32)
  const auto process_id = static_cast<std::uint64_t>(_getpid());
#else
  const auto process_id = static_cast<std::uint64_t>(getpid());
#endif
  auto temporary = path;
  temporary += ".tmp." + std::to_string(process_id) + "." +
               std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream)
      return false;
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    stream.flush();
    if (!stream) {
      stream.close();
      std::error_code error;
      std::filesystem::remove(temporary, error);
      return false;
    }
  }
  if (replace_file(temporary, path))
    return true;
  std::error_code error;
  std::filesystem::remove(temporary, error);
  return std::ranges::equal(read_file(path), bytes);
}

std::filesystem::path sidecar_path(const std::filesystem::path& manifest, std::string_view suffix) {
  auto result = manifest;
  result += suffix;
  return result;
}

} // namespace

std::string shader_file_sha256(const std::filesystem::path& path) noexcept {
  try {
    const auto bytes = read_file(path);
    if (bytes.empty())
      return {};
    const auto hash = shader_bytes_sha256(bytes);
    constexpr char digits[] = "0123456789abcdef";
    std::string result(hash.size() * 2, '0');
    for (std::size_t index = 0; index < hash.size(); ++index) {
      const auto value = std::to_integer<unsigned char>(hash[index]);
      result[index * 2] = digits[value >> 4U];
      result[index * 2 + 1] = digits[value & 0x0fU];
    }
    return result;
  } catch (...) {
    return {};
  }
}

shader_asset_error store_shader_asset(const std::filesystem::path& path,
                                      std::span<const std::byte> manifest, std::string_view wgsl,
                                      std::span<const std::byte> spirv, bool& cache_hit) noexcept {
  cache_hit = false;
  shader_asset_view view;
  if (decode_shader_asset(manifest, view) != shader_asset_error::success ||
      validate_shader_asset_payloads(view, wgsl, spirv) != shader_asset_error::success)
    return shader_asset_error::invalid_argument;
  try {
    const auto wgsl_path = sidecar_path(path, ".wgsl");
    const auto spirv_path = sidecar_path(path, ".spv");
    const auto wgsl_bytes = std::span{reinterpret_cast<const std::byte*>(wgsl.data()), wgsl.size()};
    const auto has_wgsl = find_shader_asset_variant(view, shader_asset_backend::webgpu,
                                                    shader_asset_profile::portable) != nullptr;
    const auto has_spirv = find_shader_asset_variant(view, shader_asset_backend::vulkan,
                                                     shader_asset_profile::portable) != nullptr;
    if (std::ranges::equal(read_file(path), manifest) &&
        (!has_wgsl || std::ranges::equal(read_file(wgsl_path), wgsl_bytes)) &&
        (!has_spirv || std::ranges::equal(read_file(spirv_path), spirv)) &&
        (has_wgsl || !std::filesystem::exists(wgsl_path)) &&
        (has_spirv || !std::filesystem::exists(spirv_path))) {
      cache_hit = true;
      return shader_asset_error::success;
    }
    std::error_code error;
    if (!path.parent_path().empty())
      std::filesystem::create_directories(path.parent_path(), error);
    if (error)
      return shader_asset_error::invalid_argument;
    // 清单最后替换；中途失败时旧清单不会错误匹配新旧混合载荷。
    if ((has_wgsl && !write_file_atomically(wgsl_path, wgsl_bytes)) ||
        (has_spirv && !write_file_atomically(spirv_path, spirv)))
      return shader_asset_error::invalid_argument;
    if (!has_wgsl)
      std::filesystem::remove(wgsl_path, error);
    if (!has_spirv)
      std::filesystem::remove(spirv_path, error);
    if (error || !write_file_atomically(path, manifest))
      return shader_asset_error::invalid_argument;
    return shader_asset_error::success;
  } catch (...) {
    return shader_asset_error::invalid_argument;
  }
}

} // namespace granit::tools
