# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

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

# 添加 Granit 内部 Math 模块。
# 调用方必须先定义 granit::granit；重复调用不会创建第二套目标。
function(granit_add_math_module)
  if(TARGET granit_math)
    return()
  endif()
  if(NOT TARGET granit::granit)
    message(FATAL_ERROR "创建 Math 模块前必须先定义 granit::granit")
  endif()

  add_library(granit_math STATIC)
  add_library(granit::math ALIAS granit_math)
  target_sources(
    granit_math
    PRIVATE "${PROJECT_SOURCE_DIR}/src/math/math.cpp"
    PRIVATE
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
      FILES "${PROJECT_SOURCE_DIR}/src/math/math.h"
  )
  target_compile_features(granit_math PUBLIC cxx_std_20)
  target_include_directories(
    granit_math PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
  )
  target_link_libraries(granit_math PUBLIC granit::granit)
  granit_target_compile_warnings(granit_math)
  granit_target_output_directories(granit_math)
  set_target_properties(
    granit_math
    PROPERTIES
      EXPORT_NAME detail_math
      FOLDER "Modules"
      POSITION_INDEPENDENT_CODE ON
  )
endfunction()

# 添加 Granit 内部 Material 模块。
# 该模块通过函数依赖 Math，调用方不需要关心创建顺序。
function(granit_add_material_module)
  if(TARGET granit_material)
    return()
  endif()
  granit_add_math_module()

  add_library(granit_material STATIC)
  add_library(granit::material ALIAS granit_material)
  target_sources(
    granit_material
    PRIVATE
      "${PROJECT_SOURCE_DIR}/src/material/material_archive.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/material_gpu_instance.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/material_hot_reload.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/material_metadata.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/material_migration.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/material_package.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/material_package_archive.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/pbr_default_resources.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/pbr_draw_inputs.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/pbr_material_schema.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/pbr_reference.cpp"
      "${PROJECT_SOURCE_DIR}/src/material/material_template_gpu.cpp"
    PRIVATE
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
      FILES
        "${PROJECT_SOURCE_DIR}/src/material/material_archive.h"
        "${PROJECT_SOURCE_DIR}/src/material/material_gpu_instance.h"
        "${PROJECT_SOURCE_DIR}/src/material/material_hot_reload.h"
        "${PROJECT_SOURCE_DIR}/src/material/material_metadata.h"
        "${PROJECT_SOURCE_DIR}/src/material/material_migration.h"
        "${PROJECT_SOURCE_DIR}/src/material/material_package.h"
        "${PROJECT_SOURCE_DIR}/src/material/material_package_archive.h"
        "${PROJECT_SOURCE_DIR}/src/material/pbr_default_resources.h"
        "${PROJECT_SOURCE_DIR}/src/material/pbr_draw_inputs.h"
        "${PROJECT_SOURCE_DIR}/src/material/pbr_material_schema.h"
        "${PROJECT_SOURCE_DIR}/src/material/pbr_reference.h"
        "${PROJECT_SOURCE_DIR}/src/material/material_template_gpu.h"
  )
  target_compile_features(granit_material PUBLIC cxx_std_20)
  target_include_directories(
    granit_material PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
  )
  target_link_libraries(granit_material PUBLIC granit::granit granit::math)
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
      "${PROJECT_SOURCE_DIR}/src/lighting/ibl_reference.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/ibl_resources.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/light_buffers.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/light_data.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/lighting_reference.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/reference_pipeline_graph.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/shadow_resources.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/shadow_ibl_resources.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/shadow_reference.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_reference.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_pass.cpp"
      "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_resources.cpp"
    PRIVATE
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
      FILES
        "${PROJECT_SOURCE_DIR}/src/lighting/directional_shadow.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/ibl_reference.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/ibl_resources.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/light_buffers.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/light_data.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/lighting_reference.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/reference_pipeline_graph.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/shadow_resources.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/shadow_ibl_resources.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/shadow_reference.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_reference.h"
        "${PROJECT_SOURCE_DIR}/src/lighting/tone_mapping_pass.h"
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
