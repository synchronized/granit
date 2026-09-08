# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include(granit_core_sources)

add_library(
  granit STATIC
  $<TARGET_OBJECTS:granit_shader_asset_format>
  ${GRANIT_CORE_SOURCES}
  ${GRANIT_WEBGPU_BACKEND_SOURCES}
)
add_library(granit::granit ALIAS granit)
set(GRANIT_WEB_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${GRANIT_WEB_GENERATED_INCLUDE_DIR}/granit/core")
configure_file(
  "${PROJECT_SOURCE_DIR}/include/granit/core/version.h.in"
  "${GRANIT_WEB_GENERATED_INCLUDE_DIR}/granit/core/version.h"
  @ONLY
)
target_compile_features(granit PUBLIC cxx_std_20)
target_include_directories(
  granit
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
         "$<BUILD_INTERFACE:${GRANIT_WEB_GENERATED_INCLUDE_DIR}>"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_compile_definitions(granit PUBLIC GRANIT_STATIC_DEFINE)
target_compile_options(granit PRIVATE "--use-port=emdawnwebgpu:cpp_bindings=false")
target_link_options(granit INTERFACE "--use-port=emdawnwebgpu:cpp_bindings=false")
granit_target_compile_warnings(granit)

# Web 与桌面复用同一 Math 模块目标，为完整 Model Viewer Core 提供一致依赖。
include(granit_modules)
granit_add_render_pipeline_module()
