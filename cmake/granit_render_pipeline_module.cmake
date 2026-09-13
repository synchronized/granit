# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 添加 Granit Render Pipeline 模块及其生成资产。
function(granit_add_render_pipeline_module)
  if(TARGET granit_render_pipeline)
    return()
  endif()
  granit_add_lighting_module()

  add_library(granit_render_pipeline)
  add_library(granit::render_pipeline ALIAS granit_render_pipeline)
  set(granit_pipeline_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/pipeline")
  # 本机构建生成并校验快照；交叉构建与轻量 Consumer 直接使用已校验快照。
  set(granit_pipeline_generate_shader_libraries FALSE)
  if(NOT CMAKE_CROSSCOMPILING AND
     (GRANIT_BUILD_TOOLS OR GRANIT_BUILD_ASSET_TOOLS OR GRANIT_BUILD_EXAMPLES OR
      GRANIT_BUILD_BENCHMARKS OR (GRANIT_BUILD_TESTING AND BUILD_TESTING)) AND
     GRANIT_DXC_EXECUTABLE AND GRANIT_TINT_EXECUTABLE)
    set(granit_pipeline_generate_shader_libraries TRUE)
  endif()
  if(granit_pipeline_generate_shader_libraries)
    set(granit_pipeline_builtin_archive
        "${granit_pipeline_generated_dir}/render_pipeline_builtin.grshlib")
    set(granit_pipeline_builtin_index
        "${granit_pipeline_generated_dir}/render_pipeline_builtin.grshidx.json")
    granit_add_hlsl_shader_library(
      NAME render_pipeline_builtin
      MANIFEST
        "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/render_pipeline_builtin.grshlib.json"
      OUTPUT "${granit_pipeline_builtin_archive}"
      INDEX "${granit_pipeline_builtin_index}"
      CACHE_DIR "${granit_pipeline_generated_dir}/cache/render_pipeline_builtin"
      REFERENCE "${PROJECT_SOURCE_DIR}/src/pipeline/assets/render_pipeline_builtin.grshlib"
      INDEX_REFERENCE
        "${PROJECT_SOURCE_DIR}/src/pipeline/assets/render_pipeline_builtin.grshidx.json"
      TARGET granit_pipeline_builtin_shader_library
      SOURCES
        "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.hlsl"
        "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/shadow_depth.hlsl")
    set(granit_pipeline_debug_archive "${granit_pipeline_generated_dir}/debug_draw.grshlib")
    set(granit_pipeline_debug_index "${granit_pipeline_generated_dir}/debug_draw.grshidx.json")
    granit_add_hlsl_shader_library(
      NAME debug_draw
      MANIFEST "${PROJECT_SOURCE_DIR}/assets/shaders/debug/debug_draw.grshlib.json"
      OUTPUT "${granit_pipeline_debug_archive}"
      INDEX "${granit_pipeline_debug_index}"
      CACHE_DIR "${granit_pipeline_generated_dir}/cache/debug_draw"
      REFERENCE "${PROJECT_SOURCE_DIR}/src/pipeline/assets/debug_draw.grshlib"
      INDEX_REFERENCE "${PROJECT_SOURCE_DIR}/src/pipeline/assets/debug_draw.grshidx.json"
      TARGET granit_pipeline_debug_shader_library
      SOURCES "${PROJECT_SOURCE_DIR}/assets/shaders/debug/world.hlsl")
    set(granit_pipeline_builtin_library_dependencies granit_pipeline_builtin_shader_library
                                                     "${granit_pipeline_builtin_archive}")
    set(granit_pipeline_debug_library_dependencies granit_pipeline_debug_shader_library
                                                   "${granit_pipeline_debug_archive}")
  else()
    set(granit_pipeline_builtin_archive
        "${PROJECT_SOURCE_DIR}/src/pipeline/assets/render_pipeline_builtin.grshlib")
    set(granit_pipeline_debug_archive
        "${PROJECT_SOURCE_DIR}/src/pipeline/assets/debug_draw.grshlib")
    set(granit_pipeline_builtin_library_dependencies "${granit_pipeline_builtin_archive}")
    set(granit_pipeline_debug_library_dependencies "${granit_pipeline_debug_archive}")
  endif()

  set(granit_pipeline_builtin_library
      "${granit_pipeline_generated_dir}/render_pipeline_builtin.grshlib.inc")
  set(granit_pipeline_debug_library
      "${granit_pipeline_generated_dir}/debug_draw.grshlib.inc")
  foreach(kind builtin debug)
    add_custom_command(
      OUTPUT "${granit_pipeline_${kind}_library}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
      COMMAND "${CMAKE_COMMAND}" "-DINPUT=${granit_pipeline_${kind}_archive}"
              "-DOUTPUT=${granit_pipeline_${kind}_library}"
              -P "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
      DEPENDS ${granit_pipeline_${kind}_library_dependencies}
              "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
      VERBATIM)
  endforeach()

  set(granit_pipeline_builtin_shader_ids
      "${granit_pipeline_generated_dir}/render_pipeline_shader_ids.inc")
  set(granit_pipeline_debug_shader_ids
      "${granit_pipeline_generated_dir}/debug_draw_shader_ids.inc")
  if(granit_pipeline_generate_shader_libraries)
    add_custom_command(
      OUTPUT "${granit_pipeline_builtin_shader_ids}"
      COMMAND "$<TARGET_FILE:granit_asset_tool>" shader index-ids
              --index "${granit_pipeline_builtin_index}"
              --shader tone_mapping_vertex_id=tone_mapping.vertex
              --shader tone_mapping_fragment_id=tone_mapping.fragment
              --shader shadow_depth_vertex_id=shadow_depth.vertex
              --shader shadow_depth_fragment_id=shadow_depth.fragment
              --output "${granit_pipeline_builtin_shader_ids}"
      COMMAND "${CMAKE_COMMAND}" -E compare_files "${granit_pipeline_builtin_shader_ids}"
              "${PROJECT_SOURCE_DIR}/src/pipeline/assets/render_pipeline_shader_ids.inc"
      DEPENDS
        granit_asset_tool
        granit_pipeline_builtin_shader_library
        "${granit_pipeline_builtin_index}"
        "${PROJECT_SOURCE_DIR}/src/pipeline/assets/render_pipeline_shader_ids.inc"
      VERBATIM)
    add_custom_command(
      OUTPUT "${granit_pipeline_debug_shader_ids}"
      COMMAND "$<TARGET_FILE:granit_asset_tool>" shader index-ids
              --index "${granit_pipeline_debug_index}"
              --shader debug_world_vertex_id=world.vertex
              --shader debug_world_fragment_id=world.fragment/linear
              --shader debug_world_srgb_fragment_id=world.fragment/encode_srgb
              --output "${granit_pipeline_debug_shader_ids}"
      COMMAND "${CMAKE_COMMAND}" -E compare_files "${granit_pipeline_debug_shader_ids}"
              "${PROJECT_SOURCE_DIR}/src/pipeline/assets/debug_draw_shader_ids.inc"
      DEPENDS
        granit_asset_tool
        granit_pipeline_debug_shader_library
        "${granit_pipeline_debug_index}"
        "${PROJECT_SOURCE_DIR}/src/pipeline/assets/debug_draw_shader_ids.inc"
      VERBATIM)
  else()
    add_custom_command(
      OUTPUT "${granit_pipeline_builtin_shader_ids}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${PROJECT_SOURCE_DIR}/src/pipeline/assets/render_pipeline_shader_ids.inc"
              "${granit_pipeline_builtin_shader_ids}"
      DEPENDS "${PROJECT_SOURCE_DIR}/src/pipeline/assets/render_pipeline_shader_ids.inc"
      VERBATIM)
    add_custom_command(
      OUTPUT "${granit_pipeline_debug_shader_ids}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${PROJECT_SOURCE_DIR}/src/pipeline/assets/debug_draw_shader_ids.inc"
              "${granit_pipeline_debug_shader_ids}"
      DEPENDS "${PROJECT_SOURCE_DIR}/src/pipeline/assets/debug_draw_shader_ids.inc"
      VERBATIM)
  endif()
  set(
    granit_pipeline_canvas_material
    "${granit_pipeline_generated_dir}/granit_pipeline_canvas.grmat.inc"
  )
  set(granit_pipeline_canvas_shader_library
      "${granit_pipeline_generated_dir}/unlit_canvas.grshlib.inc")
  add_custom_command(
    OUTPUT "${granit_pipeline_canvas_shader_library}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DINPUT=${PROJECT_SOURCE_DIR}/src/pipeline/assets/unlit_canvas.grshlib"
      "-DOUTPUT=${granit_pipeline_canvas_shader_library}"
      -P "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    DEPENDS
      "${PROJECT_SOURCE_DIR}/src/pipeline/assets/unlit_canvas.grshlib"
      "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    VERBATIM
  )
  add_custom_command(
    OUTPUT "${granit_pipeline_canvas_material}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DINPUT=${PROJECT_SOURCE_DIR}/src/pipeline/assets/unlit_canvas.grmat"
      "-DOUTPUT=${granit_pipeline_canvas_material}"
      -P "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    DEPENDS
      "${PROJECT_SOURCE_DIR}/src/pipeline/assets/unlit_canvas.grmat"
      "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    VERBATIM
  )
  target_sources(
    granit_render_pipeline
    PRIVATE
      "${granit_pipeline_builtin_library}"
      "${granit_pipeline_debug_library}"
      "${granit_pipeline_builtin_shader_ids}"
      "${granit_pipeline_debug_shader_ids}"
      "${granit_pipeline_canvas_material}"
      "${granit_pipeline_canvas_shader_library}"
      "${PROJECT_SOURCE_DIR}/src/pipeline/embedded_shaders.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/embedded_shaders.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/forward_draw_recorder.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/forward_draw_recorder.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/default_ibl_resources.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/default_ibl_resources.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/dynamic_uniform_arena.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/dynamic_uniform_arena.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/draw_binding_cache.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/environment_asset.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/environment_asset.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/environment_map_api.cpp"
      $<TARGET_OBJECTS:granit_internal_shader_format>
      "${PROJECT_SOURCE_DIR}/src/pipeline/material_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/material_access.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/lighting_submission.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/lighting_submission.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/mesh_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/mesh_access.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/pbr_draw_bindings.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/pbr_draw_bindings.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/pbr_material_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/render_pipeline_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/render_pipeline_metrics.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/render_pipeline_metrics.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/render_pipeline_state.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/render_view_submission.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/render_view_submission.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/scene_access.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/scene_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/shadow_draw_recorder.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/shadow_draw_recorder.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/unlit_pass.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/unlit_pass.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/canvas_draw_list.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/canvas_draw_list_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/canvas_geometry_upload.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/canvas_material_group_cache.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/canvas_material_group_cache.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/canvas_pass.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/canvas_pass.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/debug_draw_list_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/debug_draw_geometry.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/debug_draw_geometry.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/text_draw_list_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/text_atlas_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/tone_mapping_recorder.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/tone_mapping_recorder.h"
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
      FILES
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/export.h"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/environment_map.h"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/environment_map.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/material.h"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/material.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/mesh.h"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/mesh.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/pbr_material.h"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/pbr_material.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/render_pipeline.h"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/render_pipeline.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/scene.h"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/scene.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/canvas_draw_list.h"
          "${PROJECT_SOURCE_DIR}/include/granit/pipeline/canvas_draw_list.hpp"
          "${PROJECT_SOURCE_DIR}/include/granit/pipeline/debug_draw_list.h"
          "${PROJECT_SOURCE_DIR}/include/granit/pipeline/debug_draw_list.hpp"
          "${PROJECT_SOURCE_DIR}/include/granit/pipeline/text_draw_list.h"
          "${PROJECT_SOURCE_DIR}/include/granit/pipeline/text_draw_list.hpp"
          "${PROJECT_SOURCE_DIR}/include/granit/pipeline/text_atlas.h"
          "${PROJECT_SOURCE_DIR}/include/granit/pipeline/text_atlas.hpp"
  )
  target_compile_features(granit_render_pipeline PUBLIC cxx_std_20)
  target_include_directories(
    granit_render_pipeline
    PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    PRIVATE
      "${PROJECT_SOURCE_DIR}/src"
      "${granit_pipeline_generated_dir}"
  )
  target_link_libraries(granit_render_pipeline PUBLIC granit::granit PRIVATE granit::lighting)
  target_compile_definitions(
    granit_render_pipeline
    PRIVATE GRANIT_RENDER_PIPELINE_BUILDING_LIBRARY
    PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_RENDER_PIPELINE_STATIC_DEFINE>
  )
  granit_target_compile_warnings(granit_render_pipeline)
  granit_target_output_directories(granit_render_pipeline)
  set_target_properties(
    granit_render_pipeline
    PROPERTIES
      EXPORT_NAME render_pipeline
      FOLDER "Modules"
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES
  )
endfunction()
