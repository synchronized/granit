# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(GRANIT_HAS_NATIVE_TOOLS AND GRANIT_SHADER_TOOLCHAIN_ROOT AND GRANIT_DXC_EXECUTABLE AND
   GRANIT_TINT_EXECUTABLE AND TARGET granit::integration_imgui)
  add_subdirectory(01_cube)
  add_subdirectory(06_material_and_lighting)
  add_subdirectory(07_render_pipeline)
  if(NOT EMSCRIPTEN AND TARGET granit::integration_imgui)
    add_subdirectory(08_imgui)
  endif()
endif()

if(EMSCRIPTEN AND TARGET granit::integration_imgui)
  add_subdirectory(01_cube)
endif()

if(GRANIT_HAS_NATIVE_WINDOW)
  add_subdirectory(09_model_loading)
endif()

add_subdirectory(10_model_viewer)

if(EMSCRIPTEN AND TARGET granit::integration_imgui)
  add_subdirectory(08_imgui)
endif()
