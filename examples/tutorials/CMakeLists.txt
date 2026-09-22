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
endif()
