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
     (GRANIT_BUILD_TOOLS OR GRANIT_BUILD_SHADER_TOOLS OR GRANIT_BUILD_EXAMPLES OR
      GRANIT_BUILD_BENCHMARKS OR (GRANIT_BUILD_TESTING AND BUILD_TESTING)))
    set(granit_pipeline_generate_shader_libraries TRUE)
  endif()
  if(granit_pipeline_generate_shader_libraries)
    set(granit_pipeline_shader_asset_dir "${granit_pipeline_generated_dir}/shader-assets")
    granit_add_packed_shader_asset(
      NAME tone_mapping.vert
      SPIRV "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.vert.spv"
      WGSL "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.wgsl"
      ENTRY vertex_main
      STAGE vertex
      OUTPUT_DIR "${granit_pipeline_shader_asset_dir}"
      OUTPUT_VAR granit_pipeline_tone_vertex_outputs)
    list(GET granit_pipeline_tone_vertex_outputs 0 granit_pipeline_tone_vertex_asset)
    granit_add_packed_shader_asset(
      NAME tone_mapping.frag
      SPIRV "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.frag.spv"
      WGSL "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.wgsl"
      ENTRY fragment_main
      STAGE fragment
      OUTPUT_DIR "${granit_pipeline_shader_asset_dir}"
      OUTPUT_VAR granit_pipeline_tone_fragment_outputs)
    list(GET granit_pipeline_tone_fragment_outputs 0 granit_pipeline_tone_fragment_asset)
    granit_add_packed_shader_asset(
      NAME shadow_depth.vert
      SPIRV "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/shadow_depth.vert.spv"
      WGSL "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/shadow_depth.vert.wgsl"
      ENTRY vertex_main
      STAGE vertex
      OUTPUT_DIR "${granit_pipeline_shader_asset_dir}"
      OUTPUT_VAR granit_pipeline_shadow_vertex_outputs)
    list(GET granit_pipeline_shadow_vertex_outputs 0 granit_pipeline_shadow_vertex_asset)
    granit_add_packed_shader_asset(
      NAME shadow_depth.frag
      SPIRV "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/shadow_depth.frag.spv"
      WGSL "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/shadow_depth.frag.wgsl"
      ENTRY fragment_main
      STAGE fragment
      OUTPUT_DIR "${granit_pipeline_shader_asset_dir}"
      OUTPUT_VAR granit_pipeline_shadow_fragment_outputs)
    list(GET granit_pipeline_shadow_fragment_outputs 0 granit_pipeline_shadow_fragment_asset)

    set(granit_pipeline_debug_assets)
    foreach(name world.vert world.frag world_encode_srgb.frag)
      if(name STREQUAL "world.vert")
        set(stage vertex)
        set(entry vertex_main)
      else()
        set(stage fragment)
        set(entry fragment_main)
      endif()
      granit_add_packed_shader_asset(
        NAME "debug_${name}"
        SPIRV "${PROJECT_SOURCE_DIR}/assets/shaders/debug/${name}.spv"
        WGSL "${PROJECT_SOURCE_DIR}/assets/shaders/debug/${name}.wgsl"
        ENTRY "${entry}"
        STAGE "${stage}"
        OUTPUT_DIR "${granit_pipeline_shader_asset_dir}"
        OUTPUT_VAR output)
      list(GET output 0 asset)
      list(APPEND granit_pipeline_debug_assets "${asset}")
      string(REPLACE "." "_" id_name "debug_${name}")
      set("granit_pipeline_${id_name}_asset" "${asset}")
    endforeach()

    set(granit_pipeline_builtin_archive
        "${granit_pipeline_generated_dir}/render_pipeline_builtin.grshlib")
    granit_add_shader_library(
      NAME render_pipeline_builtin
      OUTPUT "${granit_pipeline_builtin_archive}"
      REFERENCE "${PROJECT_SOURCE_DIR}/src/pipeline/assets/render_pipeline_builtin.grshlib"
      TARGET granit_pipeline_builtin_shader_library
      ASSETS
        "${granit_pipeline_tone_vertex_asset}"
        "${granit_pipeline_tone_fragment_asset}"
        "${granit_pipeline_shadow_vertex_asset}"
        "${granit_pipeline_shadow_fragment_asset}")
    set(granit_pipeline_debug_archive "${granit_pipeline_generated_dir}/debug_draw.grshlib")
    granit_add_shader_library(
      NAME debug_draw
      OUTPUT "${granit_pipeline_debug_archive}"
      REFERENCE "${PROJECT_SOURCE_DIR}/src/pipeline/assets/debug_draw.grshlib"
      TARGET granit_pipeline_debug_shader_library
      ASSETS ${granit_pipeline_debug_assets})
    set(granit_pipeline_builtin_library_dependencies granit_pipeline_builtin_shader_library)
    set(granit_pipeline_debug_library_dependencies granit_pipeline_debug_shader_library)
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

  set(granit_pipeline_shader_ids "${granit_pipeline_generated_dir}/embedded_shader_ids.inc")
  if(granit_pipeline_generate_shader_libraries)
    add_custom_command(
      OUTPUT "${granit_pipeline_shader_ids}"
      COMMAND "$<TARGET_FILE:granit_shader_tool>" asset-ids
              --asset "tone_mapping_vertex_id=${granit_pipeline_tone_vertex_asset}"
              --asset "tone_mapping_fragment_id=${granit_pipeline_tone_fragment_asset}"
              --asset "shadow_depth_vertex_id=${granit_pipeline_shadow_vertex_asset}"
              --asset "shadow_depth_fragment_id=${granit_pipeline_shadow_fragment_asset}"
              --asset "debug_world_vertex_id=${granit_pipeline_debug_world_vert_asset}"
              --asset "debug_world_fragment_id=${granit_pipeline_debug_world_frag_asset}"
              --asset
              "debug_world_srgb_fragment_id=${granit_pipeline_debug_world_encode_srgb_frag_asset}"
              --output "${granit_pipeline_shader_ids}"
      COMMAND "${CMAKE_COMMAND}" -E compare_files "${granit_pipeline_shader_ids}"
              "${PROJECT_SOURCE_DIR}/src/pipeline/assets/embedded_shader_ids.inc"
      DEPENDS
        granit_shader_tool
        "${granit_pipeline_tone_vertex_asset}"
        "${granit_pipeline_tone_fragment_asset}"
        "${granit_pipeline_shadow_vertex_asset}"
        "${granit_pipeline_shadow_fragment_asset}"
        ${granit_pipeline_debug_assets}
        "${PROJECT_SOURCE_DIR}/src/pipeline/assets/embedded_shader_ids.inc"
      VERBATIM)
  else()
    add_custom_command(
      OUTPUT "${granit_pipeline_shader_ids}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${PROJECT_SOURCE_DIR}/src/pipeline/assets/embedded_shader_ids.inc"
              "${granit_pipeline_shader_ids}"
      DEPENDS "${PROJECT_SOURCE_DIR}/src/pipeline/assets/embedded_shader_ids.inc"
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
      ${granit_pipeline_shader_ids}
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
