// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/asset_tools.h>

int main(void) {
  granit_asset_tools_shader_target_capabilities capabilities =
      GRANIT_ASSET_TOOLS_SHADER_TARGET_CAPABILITIES_INIT;
  return granit_asset_tools_shader_get_target_capabilities(GRANIT_SHADER_BACKEND_VULKAN_BIT,
                                                           GRANIT_SHADER_PROFILE_PORTABLE,
                                                           &capabilities) == GRANIT_SUCCESS
             ? 0
             : 1;
}
