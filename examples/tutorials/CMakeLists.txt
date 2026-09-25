# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(GRANIT_HAS_NATIVE_TOOLS AND GRANIT_SHADER_TOOLCHAIN_ROOT AND GRANIT_DXC_EXECUTABLE AND
   GRANIT_TINT_EXECUTABLE)
  if(TARGET granit::integration_imgui)
    add_subdirectory(01_cube)
    add_subdirectory(02_pbr_assets)
  endif()
  add_subdirectory(03_instancing)
  add_subdirectory(04_raymarch)
  add_subdirectory(05_metaballs)
endif()

if(EMSCRIPTEN)
  if(TARGET granit::integration_imgui)
    add_subdirectory(01_cube)
    add_subdirectory(02_pbr_assets)
  endif()
  add_subdirectory(03_instancing)
  add_subdirectory(04_raymarch)
  add_subdirectory(05_metaballs)
endif()
