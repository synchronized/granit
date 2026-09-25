// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/asset_tools.hpp>

int main() {
  const auto [status, capabilities] = granit::asset_tools::shader::target_capabilities(
      granit::shader_backend::vulkan, granit::shader_profile::portable);
  static_cast<void>(capabilities);
  granit::asset_tools::material::result material;
  granit::asset_tools::texture::result texture;
  granit::asset_tools::environment::result environment;
  granit::asset_tools::shader::library_result shader_library;
  const auto material_info = material.info();
  const auto texture_info = texture.info();
  const auto environment_info = environment.info();
  const auto shader_library_info = shader_library.info();
  return status == granit::result::success && !material && !texture && !environment &&
                 !shader_library && material_info.archive.empty() && texture_info.manifest.empty() &&
                 environment_info.package.empty() && shader_library_info.diagnostic.empty()
             ? 0
             : 1;
}
