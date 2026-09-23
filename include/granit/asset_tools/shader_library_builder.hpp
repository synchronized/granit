// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_LIBRARY_BUILDER_HPP_
#define GRANIT_SHADER_LIBRARY_BUILDER_HPP_

#include <granit/asset_tools/shader_library_builder.h>
#include <granit/core/result.hpp>
#include <string_view>
#include <utility>

namespace granit::asset_tools::shader {

struct source_library_desc {
  std::string_view manifest_path;
  std::string_view toolchain_root;
  std::string_view cache_path;
  std::string_view output_path;
};

inline std::pair<::granit::result, bool>
build_library_from_manifest(const source_library_desc& desc) noexcept {
  const granit_asset_tools_shader_source_library_desc native{
      .struct_size = sizeof(granit_asset_tools_shader_source_library_desc),
      .reserved = 0,
      .manifest_path = desc.manifest_path.data(),
      .manifest_path_length = desc.manifest_path.size(),
      .toolchain_root = desc.toolchain_root.data(),
      .toolchain_root_length = desc.toolchain_root.size(),
      .cache_path = desc.cache_path.data(),
      .cache_path_length = desc.cache_path.size(),
      .output_path = desc.output_path.data(),
      .output_path_length = desc.output_path.size(),
  };
  uint32_t cache_hit = 0;
  const auto status = granit_asset_tools_shader_build_library_from_manifest(&native, &cache_hit);
  return {::granit::from_native(status), cache_hit != 0};
}

} // namespace granit::asset_tools::shader

#endif
