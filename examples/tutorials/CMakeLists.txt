# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_subdirectory(01_window)

if(GRANIT_HAS_NATIVE_TOOLS AND GRANIT_SHADER_TOOLCHAIN_ROOT AND GRANIT_DXC_EXECUTABLE AND
   GRANIT_TINT_EXECUTABLE)
  add_subdirectory(02_triangle)
endif()

if(GRANIT_HAS_NATIVE_TOOLS AND GRANIT_SHADER_TOOLCHAIN_ROOT AND GRANIT_DXC_EXECUTABLE AND
   GRANIT_TINT_EXECUTABLE)
  add_subdirectory(03_texture)
  add_subdirectory(04_depth_and_camera)
  add_subdirectory(05_mesh)
  add_subdirectory(06_material_and_lighting)
  add_subdirectory(07_render_pipeline)
  if(TARGET granit::integration_imgui)
    add_subdirectory(08_imgui)
  endif()
endif()

if(GRANIT_HAS_NATIVE_WINDOW)
  add_subdirectory(09_model_loading)
endif()

add_subdirectory(10_model_viewer)

if(EMSCRIPTEN AND TARGET granit::integration_imgui)
  add_subdirectory(08_imgui)
endif()
