// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TOOLS_ASSET_FILE_IO_H_
#define GRANIT_TOOLS_ASSET_FILE_IO_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace granit::asset_tools::cli {

inline constexpr std::uint64_t maximum_input_size = UINT64_C(1024) * 1024 * 1024;

std::vector<std::byte> read_file(const std::filesystem::path& path,
                                 std::uint64_t maximum_size = maximum_input_size);
std::string read_text_file(const std::filesystem::path& path,
                           std::uint64_t maximum_size = maximum_input_size);
bool write_file_atomic(const std::filesystem::path& path, std::span<const std::byte> value);
bool write_file_atomic(const std::filesystem::path& path, std::string_view value);

} // namespace granit::asset_tools::cli

#endif
