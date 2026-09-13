// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/asset_tools.hpp>

int main() {
  const auto [status, capabilities] = granit::asset_tools::shader::target_capabilities(
      granit::shader_backend::vulkan, granit::shader_profile::portable);
  static_cast<void>(capabilities);
  granit::asset_tools::material::result material;
  granit::asset_tools::texture::result texture;
  granit::asset_tools::environment::result environment;
  return status == granit::result::success && !material && !texture && !environment ? 0 : 1;
}
