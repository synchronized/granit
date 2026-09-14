// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_tools/shader/library_builder.h"

#include "asset_formats/shader/shader_library.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <new>
#include <span>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

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

struct owned_object {
  std::vector<std::byte> manifest;
  std::vector<std::byte> wgsl;
  std::vector<std::byte> spirv;
};

} // namespace

namespace granit::asset_tools::detail {

granit_result link_shader_library(std::span<const std::filesystem::path> object_paths,
                                  granit_shader_backend_flags target_backends,
                                  const std::filesystem::path& output_path,
                                  bool& cache_hit) noexcept {
  cache_hit = false;
  if (object_paths.empty() || output_path.empty() || target_backends == 0 ||
      (target_backends & ~GRANIT_SHADER_BACKEND_ALL_BITS) != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<owned_object> owned;
    owned.reserve(object_paths.size());
    for (const auto& object_path : object_paths) {
      owned_object object{.manifest = read_file(object_path), .wgsl = {}, .spirv = {}};
      if ((target_backends & GRANIT_SHADER_BACKEND_WEBGPU_BIT) != 0) {
        auto path = object_path;
        path += ".wgsl";
        object.wgsl = read_file(path);
      }
      if ((target_backends & GRANIT_SHADER_BACKEND_VULKAN_BIT) != 0) {
        auto path = object_path;
        path += ".spv";
        object.spirv = read_file(path);
      }
      if (object.manifest.empty() ||
          ((target_backends & GRANIT_SHADER_BACKEND_WEBGPU_BIT) != 0 && object.wgsl.empty()) ||
          ((target_backends & GRANIT_SHADER_BACKEND_VULKAN_BIT) != 0 && object.spirv.empty()))
        return GRANIT_ERROR_INVALID_ARGUMENT;
      owned.push_back(std::move(object));
    }
    std::vector<granit::detail::shader_format::shader_library_object_source> sources;
    sources.reserve(owned.size());
    for (const auto& object : owned)
      sources.push_back({object.manifest, object.wgsl, object.spirv});
    std::vector<std::byte> library;
    if (granit::detail::shader_format::encode_shader_library({sources, target_backends}, library) !=
        granit::detail::shader_format::shader_library_error::success)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (std::ranges::equal(read_file(output_path), library)) {
      cache_hit = true;
      return GRANIT_SUCCESS;
    }
    std::error_code error;
    if (!output_path.parent_path().empty())
      std::filesystem::create_directories(output_path.parent_path(), error);
    if (error || !write_file_atomically(output_path, library))
      return GRANIT_ERROR_INITIALIZATION_FAILED;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

} // namespace granit::asset_tools::detail
