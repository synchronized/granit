// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ASSET_TOOLS_TOOLCHAIN_LAYOUT_H_
#define GRANIT_ASSET_TOOLS_TOOLCHAIN_LAYOUT_H_

#include <filesystem>

namespace granit::asset_tools::detail {

struct shader_toolchain_paths {
  std::filesystem::path dxc;
  std::filesystem::path tint;
};

inline shader_toolchain_paths resolve_shader_toolchain(const std::filesystem::path& root) {
#if defined(_WIN32)
  return {root / "bin" / "dxc.exe", root / "bin" / "tint.exe"};
#else
  return {root / "bin" / "dxc", root / "bin" / "tint"};
#endif
}

inline bool shader_toolchain_ready(const shader_toolchain_paths& paths) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(paths.dxc, error) || error)
    return false;
  error.clear();
  return std::filesystem::is_regular_file(paths.tint, error) && !error;
}

} // namespace granit::asset_tools::detail

#endif
