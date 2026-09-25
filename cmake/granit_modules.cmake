# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/granit_render_pipeline_module.cmake")

# 添加 Granit 内部 Render Graph 模块。
function(granit_add_render_graph_module)
  if(TARGET granit_render_graph)
    return()
  endif()
  if(NOT TARGET granit::granit)
    message(FATAL_ERROR "创建 Render Graph 模块前必须先定义 granit::granit")
  endif()

  add_library(granit_render_graph STATIC)
  add_library(granit::render_graph ALIAS granit_render_graph)
  target_sources(
    granit_render_graph
    PRIVATE
      "${PROJECT_SOURCE_DIR}/src/render_graph/graph_compiler.cpp"
      "${PROJECT_SOURCE_DIR}/src/render_graph/serial_graph.cpp"
    PRIVATE
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
      FILES
        "${PROJECT_SOURCE_DIR}/src/render_graph/graph_compiler.h"
        "${PROJECT_SOURCE_DIR}/src/render_graph/serial_graph.h"
  )
  target_compile_features(granit_render_graph PUBLIC cxx_std_20)
  target_include_directories(
    granit_render_graph PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
  )
  target_link_libraries(granit_render_graph PUBLIC granit::granit)
  granit_target_compile_warnings(granit_render_graph)
  granit_target_output_directories(granit_render_graph)
  set_target_properties(
    granit_render_graph
    PROPERTIES
      EXPORT_NAME detail_render_graph
      FOLDER "Modules"
      POSITION_INDEPENDENT_CODE ON
  )
endfunction()

# 添加 Granit 内部 Material 模块。
function(granit_add_material_module)
  if(TARGET granit_material)
    return()
  endif()
  add_library(granit_material STATIC)
  add_library(granit::material ALIAS granit_material)
  target_sources(
    granit_material
    PRIVATE
      $<TARGET_OBJECTS:granit_asset_format_shader>
      $<TARGET_OBJECTS:granit_asset_format_material>
      "${PROJECT_SOURCE_DIR}/src/material/material_gpu_instance.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/material_hot_reload.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/material_migration.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/pbr_default_resources.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/pbr_draw_inputs.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/pbr_material_schema.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/material_template_gpu.cpp"
    PRIVATE
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
      FILES
        "${PROJECT_SOURCE_DIR}/src/material/material_gpu_instance.h"
        "${PROJECT_SOURCE_DIR}/src/material/material_hot_reload.h"
        "${PROJECT_SOURCE_DIR}/src/material/material_migration.h"
        "${PROJECT_SOURCE_DIR}/src/material/pbr_default_resources.h"
        "${PROJECT_SOURCE_DIR}/src/material/pbr_draw_inputs.h"
        "${PROJECT_SOURCE_DIR}/src/material/pbr_material_schema.h"
        "${PROJECT_SOURCE_DIR}/src/material/pbr_types.h"
        "${PROJECT_SOURCE_DIR}/src/material/material_template_gpu.h"
  )
  target_compile_features(granit_material PUBLIC cxx_std_20)
  target_include_directories(
    granit_material PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
  )
  target_link_libraries(granit_material PUBLIC granit::granit)
  granit_target_compile_warnings(granit_material)
  granit_target_output_directories(granit_material)
  set_target_properties(
    granit_material
    PROPERTIES
      EXPORT_NAME detail_material
      FOLDER "Modules"
      POSITION_INDEPENDENT_CODE ON
  )
endfunction()

# 添加 Granit 内部 PBR 适配模块。
function(granit_add_pbr_module)
  if(TARGET granit_pbr)
    return()
  endif()
  granit_add_material_module()
  granit_add_render_graph_module()

  add_library(granit_pbr STATIC)
  add_library(granit::pbr ALIAS granit_pbr)
  target_sources(
    granit_pbr
    PRIVATE "${PROJECT_SOURCE_DIR}/src/material/pbr_render_graph_adapter.cpp"
    PRIVATE
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
      FILES "${PROJECT_SOURCE_DIR}/src/material/pbr_render_graph_adapter.h"
  )
  target_compile_features(granit_pbr PUBLIC cxx_std_20)
  target_include_directories(
    granit_pbr PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
  )
  target_link_libraries(granit_pbr PUBLIC granit::material granit::render_graph)
  granit_target_compile_warnings(granit_pbr)
  granit_target_output_directories(granit_pbr)
  set_target_properties(
    granit_pbr
    PROPERTIES
      EXPORT_NAME detail_pbr
      FOLDER "Modules"
      POSITION_INDEPENDENT_CODE ON
  )
endfunction()

# 添加 Granit 内部 Scene 模块。
function(granit_add_scene_module)
  if(TARGET granit_scene)
    return()
  endif()
  granit_add_pbr_module()

  add_library(granit_scene STATIC)
  add_library(granit::scene ALIAS granit_scene)
  target_sources(
    granit_scene
    PRIVATE
      "${PROJECT_SOURCE_DIR}/src/scene/multi_view_submission.cpp"
      "${PROJECT_SOURCE_DIR}/src/scene/scene_pbr_adapter.cpp"
      "${PROJECT_SOURCE_DIR}/src/scene/scene_submission.cpp"
      "${PROJECT_SOURCE_DIR}/src/scene/scene_visibility.cpp"
    PRIVATE
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
      FILES
        "${PROJECT_SOURCE_DIR}/src/scene/multi_view_submission.h"
        "${PROJECT_SOURCE_DIR}/src/scene/scene_pbr_adapter.h"
        "${PROJECT_SOURCE_DIR}/src/scene/scene_submission.h"
        "${PROJECT_SOURCE_DIR}/src/scene/scene_visibility.h"
  )
  target_compile_features(granit_scene PUBLIC cxx_std_20)
  target_include_directories(
    granit_scene PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
  )
  target_link_libraries(granit_scene PUBLIC granit::pbr)
  granit_target_compile_warnings(granit_scene)
  granit_target_output_directories(granit_scene)
  set_target_properties(
    granit_scene
    PROPERTIES
      EXPORT_NAME detail_scene
      FOLDER "Modules"
      POSITION_INDEPENDENT_CODE ON
  )
endfunction()

# 添加 Granit 内部 Lighting 模块。
function(granit_add_lighting_module)
  if(TARGET granit_lighting)
    return()
  endif()
  granit_add_scene_module()

  add_library(granit_lighting STATIC)
  add_library(granit::lighting ALIAS granit_lighting)
  target_sources(
    granit_lighting
    PRIVATE
      "${PROJECT_SOURCE_DIR}/src/lighting/directional_shadow.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/ibl_resources.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/light_buffers.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/light_data.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/forward_pipeline_graph.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/shadow_resources.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/shadow_ibl_resources.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_pass.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_types.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_resources.cpp"
    PRIVATE
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
      FILES
        "${PROJECT_SOURCE_DIR}/src/lighting/directional_shadow.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/ibl_resources.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/light_buffers.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/light_data.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/forward_pipeline_graph.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/shadow_resources.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/shadow_ibl_resources.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_pass.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_types.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_resources.h"
  )
  target_compile_features(granit_lighting PUBLIC cxx_std_20)
  target_include_directories(
    granit_lighting PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
  )
  target_link_libraries(granit_lighting PUBLIC granit::scene)
  granit_target_compile_warnings(granit_lighting)
  granit_target_output_directories(granit_lighting)
  set_target_properties(
    granit_lighting
    PROPERTIES
      EXPORT_NAME detail_lighting
      FOLDER "Modules"
      POSITION_INDEPENDENT_CODE ON
  )
endfunction()
