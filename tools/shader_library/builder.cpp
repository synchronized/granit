// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_library_builder.h>

#include "shader_format/shader_library.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
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

bool valid_string(const char* value, uint64_t length) { return value != nullptr || length == 0; }

std::filesystem::path copy_path(const char* value, uint64_t length) {
  std::u8string utf8(static_cast<std::size_t>(length), u8'\0');
  if (length != 0)
    std::memcpy(utf8.data(), value, static_cast<std::size_t>(length));
  return std::filesystem::path{utf8};
}

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

extern "C" granit_result
granit_shader_tools_build_library(const granit_shader_tools_library_desc* desc,
                                  uint32_t* cache_hit) {
  if (cache_hit == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *cache_hit = 0;
  if (desc == nullptr || desc->struct_size < sizeof(*desc) || desc->reserved != 0 ||
      desc->reserved2 != 0 || desc->objects == nullptr || desc->object_count == 0 ||
      !valid_string(desc->output_path, desc->output_path_length) || desc->output_path_length == 0 ||
      desc->target_backends == 0 || (desc->target_backends & ~GRANIT_SHADER_BACKEND_ALL_BITS) != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (uint64_t index = 0; index < desc->object_count; ++index) {
    const auto& object = desc->objects[index];
    if (object.struct_size < sizeof(object) || object.reserved != 0 ||
        !valid_string(object.path, object.path_length) || object.path_length == 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    std::vector<owned_object> owned;
    owned.reserve(static_cast<std::size_t>(desc->object_count));
    for (uint64_t index = 0; index < desc->object_count; ++index) {
      const auto object_path =
          copy_path(desc->objects[index].path, desc->objects[index].path_length);
      owned_object object{.manifest = read_file(object_path), .wgsl = {}, .spirv = {}};
      if ((desc->target_backends & GRANIT_SHADER_BACKEND_WEBGPU_BIT) != 0) {
        auto path = object_path;
        path += ".wgsl";
        object.wgsl = read_file(path);
      }
      if ((desc->target_backends & GRANIT_SHADER_BACKEND_VULKAN_BIT) != 0) {
        auto path = object_path;
        path += ".spv";
        object.spirv = read_file(path);
      }
      if (object.manifest.empty() ||
          ((desc->target_backends & GRANIT_SHADER_BACKEND_WEBGPU_BIT) != 0 &&
           object.wgsl.empty()) ||
          ((desc->target_backends & GRANIT_SHADER_BACKEND_VULKAN_BIT) != 0 && object.spirv.empty()))
        return GRANIT_ERROR_INVALID_ARGUMENT;
      owned.push_back(std::move(object));
    }
    std::vector<granit::detail::shader_format::shader_library_object_source> sources;
    sources.reserve(owned.size());
    for (const auto& object : owned)
      sources.push_back({object.manifest, object.wgsl, object.spirv});
    std::vector<std::byte> library;
    if (granit::detail::shader_format::encode_shader_library({sources, desc->target_backends},
                                                             library) !=
        granit::detail::shader_format::shader_library_error::success)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto output = copy_path(desc->output_path, desc->output_path_length);
    if (std::ranges::equal(read_file(output), library)) {
      *cache_hit = 1;
      return GRANIT_SUCCESS;
    }
    std::error_code error;
    if (!output.parent_path().empty())
      std::filesystem::create_directories(output.parent_path(), error);
    if (error || !write_file_atomically(output, library))
      return GRANIT_ERROR_INITIALIZATION_FAILED;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
