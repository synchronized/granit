# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(GRANIT_BUILD_MODEL_VIEWER_EXAMPLE)
  add_subdirectory(common/gltf)
  add_subdirectory(common/model_viewer)
  add_subdirectory(platform/desktop)
  add_subdirectory(platform/web)
endif()

add_subdirectory(samples/imgui)
