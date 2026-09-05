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
  set(
    granit_pipeline_tone_vertex
    "${granit_pipeline_generated_dir}/granit_pipeline_tone_mapping.vert.inc"
  )
  set(
    granit_pipeline_tone_fragment
    "${granit_pipeline_generated_dir}/granit_pipeline_tone_mapping.frag.inc"
  )
  set(
    granit_pipeline_shadow_vertex
    "${granit_pipeline_generated_dir}/granit_pipeline_shadow_depth.vert.inc"
  )
  set(
    granit_pipeline_shadow_fragment
    "${granit_pipeline_generated_dir}/granit_pipeline_shadow_depth.frag.inc"
  )
  set(
    granit_pipeline_canvas_material
    "${granit_pipeline_generated_dir}/granit_pipeline_canvas.grmat.inc"
  )
  set(granit_pipeline_canvas_shader_includes)
  foreach(asset IN ITEMS
      unlit_canvas.vert.grshader
      unlit_canvas.vert.grshader.spv
      unlit_canvas.vert.grshader.wgsl
      unlit_canvas.frag.grshader
      unlit_canvas.frag.grshader.spv
      unlit_canvas.frag.grshader.wgsl
      unlit_canvas_encode_srgb.frag.grshader
      unlit_canvas_encode_srgb.frag.grshader.spv
      unlit_canvas_encode_srgb.frag.grshader.wgsl)
    set(input "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/${asset}")
    set(output "${granit_pipeline_generated_dir}/${asset}.inc")
    add_custom_command(
      OUTPUT "${output}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
      COMMAND "${CMAKE_COMMAND}" "-DINPUT=${input}" "-DOUTPUT=${output}"
              -P "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
      DEPENDS "${input}" "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
      VERBATIM)
    list(APPEND granit_pipeline_canvas_shader_includes "${output}")
  endforeach()
  set(granit_pipeline_debug_world_vertex
      "${granit_pipeline_generated_dir}/granit_pipeline_debug_world.vert.inc")
  set(granit_pipeline_debug_world_fragment
      "${granit_pipeline_generated_dir}/granit_pipeline_debug_world.frag.inc")
  set(granit_pipeline_debug_world_srgb_fragment
      "${granit_pipeline_generated_dir}/granit_pipeline_debug_world_srgb.frag.inc")
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
  add_custom_command(
    OUTPUT "${granit_pipeline_tone_vertex}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DINPUT=${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.vert.spv"
      "-DOUTPUT=${granit_pipeline_tone_vertex}"
      -P "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    DEPENDS
      "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.vert.spv"
      "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    VERBATIM
  )
  foreach(granit_debug_output granit_pipeline_debug_world_vertex
                              granit_pipeline_debug_world_fragment
                              granit_pipeline_debug_world_srgb_fragment)
    if(granit_debug_output STREQUAL "granit_pipeline_debug_world_vertex")
      set(granit_debug_input "${PROJECT_SOURCE_DIR}/assets/shaders/debug/world.vert.spv")
    elseif(granit_debug_output STREQUAL "granit_pipeline_debug_world_fragment")
      set(granit_debug_input "${PROJECT_SOURCE_DIR}/assets/shaders/debug/world.frag.spv")
    else()
      set(granit_debug_input "${PROJECT_SOURCE_DIR}/assets/shaders/debug/world_encode_srgb.frag.spv")
    endif()
    add_custom_command(
      OUTPUT "${${granit_debug_output}}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
      COMMAND "${CMAKE_COMMAND}" "-DINPUT=${granit_debug_input}"
              "-DOUTPUT=${${granit_debug_output}}" -P "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
      DEPENDS "${granit_debug_input}" "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
      VERBATIM)
  endforeach()
  add_custom_command(
    OUTPUT "${granit_pipeline_tone_fragment}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DINPUT=${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.frag.spv"
      "-DOUTPUT=${granit_pipeline_tone_fragment}"
      -P "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    DEPENDS
      "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/tone_mapping.frag.spv"
      "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    VERBATIM
  )
  add_custom_command(
    OUTPUT "${granit_pipeline_shadow_vertex}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DINPUT=${PROJECT_SOURCE_DIR}/src/pipeline/shaders/shadow_depth.vert.spv"
      "-DOUTPUT=${granit_pipeline_shadow_vertex}"
      -P "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    DEPENDS
      "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/shadow_depth.vert.spv"
      "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    VERBATIM
  )
  add_custom_command(
    OUTPUT "${granit_pipeline_shadow_fragment}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_pipeline_generated_dir}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DINPUT=${PROJECT_SOURCE_DIR}/src/pipeline/shaders/shadow_depth.frag.spv"
      "-DOUTPUT=${granit_pipeline_shadow_fragment}"
      -P "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    DEPENDS
      "${PROJECT_SOURCE_DIR}/src/pipeline/shaders/shadow_depth.frag.spv"
      "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
    VERBATIM
  )
  target_sources(
    granit_render_pipeline
    PRIVATE
      "${granit_pipeline_tone_vertex}"
      "${granit_pipeline_tone_fragment}"
      "${granit_pipeline_shadow_vertex}"
      "${granit_pipeline_shadow_fragment}"
      "${granit_pipeline_canvas_material}"
      ${granit_pipeline_canvas_shader_includes}
      "${granit_pipeline_debug_world_vertex}"
      "${granit_pipeline_debug_world_fragment}"
      "${granit_pipeline_debug_world_srgb_fragment}"
      "${PROJECT_SOURCE_DIR}/src/pipeline/embedded_shaders.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/embedded_shaders.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/forward_draw_recorder.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/forward_draw_recorder.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/default_ibl_resources.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/default_ibl_resources.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/dynamic_uniform_arena.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/dynamic_uniform_arena.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/draw_binding_cache.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/material_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/material_access.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/lighting_submission.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/lighting_submission.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/mesh_api.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/mesh_access.h"
      "${PROJECT_SOURCE_DIR}/src/pipeline/pbr_draw_bindings.cpp"
      "${PROJECT_SOURCE_DIR}/src/pipeline/pbr_draw_bindings.h"
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
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/material.h"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/material.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/mesh.h"
        "${PROJECT_SOURCE_DIR}/include/granit/pipeline/mesh.hpp"
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
