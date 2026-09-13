// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/asset_tools.hpp>

int main() {
  const auto [result, capabilities] = granit::asset_tools::shader::target_capabilities(
      granit::shader_backend::vulkan, granit::shader_profile::portable);
  static_cast<void>(capabilities);
  return result == granit::result::success ? 0 : 1;
}
