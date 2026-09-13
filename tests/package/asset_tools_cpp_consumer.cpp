// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/asset_tools.hpp>

int main() {
  const auto [status, capabilities] = granit::asset_tools::shader::target_capabilities(
      granit::shader_backend::vulkan, granit::shader_profile::portable);
  static_cast<void>(capabilities);
  granit::asset_tools::material::result material;
  granit::asset_tools::texture::result texture;
  return status == granit::result::success && !material && !texture ? 0 : 1;
}
