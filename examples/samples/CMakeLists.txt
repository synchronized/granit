# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_subdirectory(imgui)

if(GRANIT_HAS_NATIVE_WINDOW)
  add_subdirectory(model_loading)
endif()

add_subdirectory(model_viewer)
