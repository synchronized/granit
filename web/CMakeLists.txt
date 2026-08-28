# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(
  granit_web_platform_smoke
  platform_smoke.cpp
  renderer_api.cpp
  renderer_registry.cpp
  shader_api.cpp
  surface_api.cpp
  swapchain_api.cpp
  "${PROJECT_SOURCE_DIR}/src/backend/plugin_loader.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/plugin.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/presentation_adapter.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/renderer_state.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/shader_adapter.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/handle_table.cpp"
  "${PROJECT_SOURCE_DIR}/src/platform/shared_library.cpp"
)
target_compile_features(granit_web_platform_smoke PRIVATE cxx_std_20)
target_include_directories(
  granit_web_platform_smoke PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
target_compile_options(
  granit_web_platform_smoke
  PRIVATE "--use-port=emdawnwebgpu:cpp_bindings=false"
)
target_link_options(
  granit_web_platform_smoke
  PRIVATE
    "--use-port=emdawnwebgpu:cpp_bindings=false"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets/s10d_startup.txt@/assets/s10d_startup.txt"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_EXIT_RUNTIME=1"
    "$<$<CONFIG:Debug>:-sASSERTIONS=1>"
    "$<$<CONFIG:Release>:--closure=1>"
)
set_target_properties(
  granit_web_platform_smoke
  PROPERTIES
    OUTPUT_NAME granit_web_platform_smoke
    SUFFIX ".html"
    FOLDER "Examples/Web"
)
granit_target_compile_warnings(granit_web_platform_smoke)
