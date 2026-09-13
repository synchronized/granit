// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_file_io.h"

#include <fstream>
#include <limits>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace granit::asset_tools::cli {
namespace {

std::filesystem::path temporary_path(const std::filesystem::path& path) {
#if defined(_WIN32)
  const auto process_id = static_cast<unsigned long>(GetCurrentProcessId());
#else
  const auto process_id = static_cast<unsigned long>(getpid());
#endif
  return std::filesystem::path{path.string() + ".tmp." + std::to_string(process_id)};
}

bool replace_file(const std::filesystem::path& temporary, const std::filesystem::path& target) {
#if defined(_WIN32)
  return MoveFileExW(temporary.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code error;
  std::filesystem::rename(temporary, target, error);
  return !error;
#endif
}

} // namespace

std::vector<std::byte> read_file(const std::filesystem::path& path,
                                 const std::uint64_t maximum_size) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    return {};
  const auto position = stream.tellg();
  if (position <= 0)
    return {};
  const auto size = static_cast<std::uint64_t>(position);
  if (size > maximum_size || size > (std::numeric_limits<std::size_t>::max)() ||
      size > static_cast<std::uint64_t>((std::numeric_limits<std::streamsize>::max)()))
    return {};
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
  return stream ? bytes : std::vector<std::byte>{};
}

std::string read_text_file(const std::filesystem::path& path, const std::uint64_t maximum_size) {
  const auto bytes = read_file(path, maximum_size);
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

bool write_file_atomic(const std::filesystem::path& path, std::span<const std::byte> value) {
  const auto temporary = temporary_path(path);
  std::error_code ignored;
  std::filesystem::remove(temporary, ignored);
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(value.data()),
                 static_cast<std::streamsize>(value.size()));
    stream.flush();
    if (!stream) {
      std::filesystem::remove(temporary, ignored);
      return false;
    }
  }
  if (!replace_file(temporary, path)) {
    std::filesystem::remove(temporary, ignored);
    return false;
  }
  return true;
}

bool write_file_atomic(const std::filesystem::path& path, const std::string_view value) {
  return write_file_atomic(path, std::as_bytes(std::span{value}));
}

} // namespace granit::asset_tools::cli
