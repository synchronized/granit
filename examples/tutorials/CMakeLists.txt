# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(GRANIT_HAS_NATIVE_TOOLS AND GRANIT_SHADER_TOOLCHAIN_ROOT AND GRANIT_DXC_EXECUTABLE AND
   GRANIT_TINT_EXECUTABLE AND TARGET granit::integration_imgui)
  add_subdirectory(01_cube)
  add_subdirectory(02_pbr_assets)
endif()

if(EMSCRIPTEN AND TARGET granit::integration_imgui)
  add_subdirectory(01_cube)
  add_subdirectory(02_pbr_assets)
endif()
