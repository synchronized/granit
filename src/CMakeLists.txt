# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(granit)
add_library(granit::granit ALIAS granit)
granit_target_output_directories(granit)

set(GRANIT_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
set(GRANIT_GENERATED_VERSION_HEADER
    "${GRANIT_GENERATED_INCLUDE_DIR}/granit/core/version.h")

file(MAKE_DIRECTORY "${GRANIT_GENERATED_INCLUDE_DIR}/granit/core")
configure_file(
  "${PROJECT_SOURCE_DIR}/include/granit/core/version.h.in"
  "${GRANIT_GENERATED_VERSION_HEADER}"
  @ONLY
)

target_sources(
  granit
  PRIVATE
    $<TARGET_OBJECTS:granit_volk>
    backend/vulkan/device.cpp
    backend/vulkan/device.h
    backend/vulkan/frame_context.cpp
    backend/vulkan/frame_context.h
    backend/vulkan/command_recorder.cpp
    backend/vulkan/command_recorder.h
    backend/vulkan/instance.cpp
    backend/vulkan/instance.h
    backend/vulkan/loader.cpp
    backend/vulkan/loader.h
    backend/vulkan/memory_allocator.cpp
    backend/vulkan/memory_allocator.h
    backend/vulkan/physical_device.cpp
    backend/vulkan/physical_device.h
    backend/vulkan/result.cpp
    backend/vulkan/result.h
    backend/vulkan/surface.cpp
    backend/vulkan/surface.h
    backend/vulkan/swapchain.cpp
    backend/vulkan/swapchain.h
    backend/vulkan/timestamp_query.cpp
    backend/vulkan/timestamp_query.h
    backend/vulkan/upload_context.cpp
    backend/vulkan/upload_context.h
    backend/vulkan/version_check.cpp
    backend/vulkan/vma_implementation.cpp
    core/handle_table.cpp
    core/handle_table.h
    core/diagnostic_sink.cpp
    core/diagnostic_sink.h
    core/lifecycle_validation.cpp
    core/lifecycle_validation.h
    core/resource_validation.cpp
    core/resource_validation.h
    core/texture_format.h
    core/retirement_queue.cpp
    core/retirement_queue.h
    core/device_status.h
    renderer/renderer_api.cpp
    renderer/pipeline_api.cpp
    renderer/buffer_api.cpp
    renderer/command_recorder_api.cpp
    renderer/renderer_registry.cpp
    renderer/renderer_registry.h
    renderer/renderer_state.cpp
    renderer/renderer_state.h
    renderer/sampler_api.cpp
    renderer/shader_api.cpp
    renderer/surface_api.cpp
    renderer/swapchain_api.cpp
    renderer/texture_api.cpp
    renderer/timestamp_query_api.cpp
    renderer/upload_batch_api.cpp
    result.cpp
    version.cpp
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      "${PROJECT_SOURCE_DIR}/include"
      "${GRANIT_GENERATED_INCLUDE_DIR}"
    FILES
      "${PROJECT_SOURCE_DIR}/include/granit/core/export.h"
      "${PROJECT_SOURCE_DIR}/include/granit/core/diagnostic.h"
      "${PROJECT_SOURCE_DIR}/include/granit/core/diagnostic.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/buffer.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/buffer.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/command_recorder.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/command_recorder.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/granit.h"
      "${PROJECT_SOURCE_DIR}/include/granit/granit.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/math/types.h"
      "${PROJECT_SOURCE_DIR}/include/granit/math/types.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/pipeline.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/pipeline.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/renderer.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/renderer.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/render_target.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/render_target.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/resource_types.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/resource_types.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/core/result.h"
      "${PROJECT_SOURCE_DIR}/include/granit/core/result.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/sampler.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/sampler.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/shader.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/shader.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/surface.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/surface.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/texture.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/texture.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/timestamp_query.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/timestamp_query.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/upload_batch.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/upload_batch.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/swapchain.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/swapchain.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/core/types.h"
      "${PROJECT_SOURCE_DIR}/include/granit/core/types.hpp"
      "${GRANIT_GENERATED_VERSION_HEADER}"
)

target_compile_features(granit PUBLIC cxx_std_20)
granit_target_compile_warnings(granit)
target_include_directories(
  granit
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${GRANIT_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(
  granit
  SYSTEM PRIVATE
    "${GRANIT_VOLK_DIR}"
    "${GRANIT_VMA_DIR}"
    "${GRANIT_VULKAN_HEADERS_DIR}"
)

target_compile_definitions(
  granit
  PRIVATE
    GRANIT_BUILDING_LIBRARY
    VK_NO_PROTOTYPES
    VOLK_NAMESPACE
    VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_VULKAN_VERSION=1003000
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
    $<$<BOOL:${GRANIT_HAS_XCB}>:GRANIT_HAS_XCB>
    $<$<BOOL:${GRANIT_HAS_XCB}>:VK_USE_PLATFORM_XCB_KHR>
    $<$<BOOL:${GRANIT_HAS_WAYLAND}>:GRANIT_HAS_WAYLAND>
    $<$<BOOL:${GRANIT_HAS_WAYLAND}>:VK_USE_PLATFORM_WAYLAND_KHR>
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_STATIC_DEFINE>
)

target_link_libraries(granit PRIVATE ${CMAKE_DL_LIBS})
target_include_directories(
  granit SYSTEM PRIVATE
    $<$<BOOL:${GRANIT_HAS_XCB}>:${GRANIT_XCB_INCLUDE_DIR}>
    $<$<BOOL:${GRANIT_HAS_WAYLAND}>:${GRANIT_WAYLAND_INCLUDE_DIR}>
)

target_compile_options(
  granit
  PUBLIC
    $<$<COMPILE_LANG_AND_ID:C,MSVC>:/utf-8>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)

set_target_properties(
  granit
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

install(
  TARGETS granit
  EXPORT granitTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
  EXPORT granitTargets
  NAMESPACE granit::
  DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
)

add_library(granit_window)
add_library(granit::window ALIAS granit_window)
target_sources(
  granit_window
  PRIVATE window/window_api.cpp
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/granit/window.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/window/export.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window/window.h"
      "${PROJECT_SOURCE_DIR}/include/granit/window/window.hpp"
)
target_compile_features(granit_window PUBLIC cxx_std_20)
target_include_directories(
  granit_window PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_compile_definitions(
  granit_window
  PRIVATE GRANIT_WINDOW_BUILDING_LIBRARY
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_WINDOW_STATIC_DEFINE>
)
granit_target_compile_warnings(granit_window)
granit_target_output_directories(granit_window)
set_target_properties(
  granit_window
  PROPERTIES
    EXPORT_NAME window
    FOLDER "Modules"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

install(
  TARGETS granit_window
  EXPORT granitWindowTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Window
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Window
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT Window
  FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT Window
)
install(
  EXPORT granitWindowTargets
  NAMESPACE granit::
  DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
  COMPONENT Window
)

add_library(granit_render_graph STATIC)
add_library(granit::render_graph ALIAS granit_render_graph)
target_sources(
  granit_render_graph
  PRIVATE
    render_graph/graph_compiler.cpp
    render_graph/serial_graph.cpp
  PRIVATE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
    FILES
      render_graph/graph_compiler.h
      render_graph/serial_graph.h
)
target_compile_features(granit_render_graph PUBLIC cxx_std_20)
target_include_directories(granit_render_graph PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>")
target_link_libraries(granit_render_graph PUBLIC granit::granit)
granit_target_compile_warnings(granit_render_graph)
granit_target_output_directories(granit_render_graph)
set_target_properties(
  granit_render_graph
  PROPERTIES FOLDER "Modules" POSITION_INDEPENDENT_CODE ON
)

add_library(granit_math STATIC)
add_library(granit::math ALIAS granit_math)
target_sources(
  granit_math
  PRIVATE math/math.cpp
  PRIVATE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
    FILES math/math.h
)
target_compile_features(granit_math PUBLIC cxx_std_20)
target_include_directories(granit_math PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>")
target_link_libraries(granit_math PUBLIC granit::granit)
granit_target_compile_warnings(granit_math)
granit_target_output_directories(granit_math)
set_target_properties(granit_math PROPERTIES FOLDER "Modules" POSITION_INDEPENDENT_CODE ON)

add_library(granit_material STATIC)
add_library(granit::material ALIAS granit_material)
target_sources(
  granit_material
  PRIVATE
    material/material_archive.cpp
    material/material_gpu_instance.cpp
    material/material_hot_reload.cpp
    material/material_metadata.cpp
    material/material_migration.cpp
    material/material_package.cpp
    material/material_package_archive.cpp
    material/pbr_default_resources.cpp
    material/pbr_draw_inputs.cpp
    material/pbr_material_schema.cpp
    material/pbr_reference.cpp
    material/material_template_gpu.cpp
  PRIVATE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
    FILES
      material/material_archive.h
      material/material_gpu_instance.h
      material/material_hot_reload.h
      material/material_metadata.h
      material/material_migration.h
      material/material_package.h
      material/material_package_archive.h
      material/pbr_default_resources.h
      material/pbr_draw_inputs.h
      material/pbr_material_schema.h
      material/pbr_reference.h
      material/material_template_gpu.h
)
target_compile_features(granit_material PUBLIC cxx_std_20)
target_include_directories(granit_material PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>")
target_link_libraries(granit_material PUBLIC granit::granit granit::math)
granit_target_compile_warnings(granit_material)
granit_target_output_directories(granit_material)
set_target_properties(
  granit_material
  PROPERTIES
    FOLDER "Modules"
    POSITION_INDEPENDENT_CODE ON
)

add_library(granit_pbr STATIC)
add_library(granit::pbr ALIAS granit_pbr)
target_sources(
  granit_pbr
  PRIVATE material/pbr_render_graph_adapter.cpp
  PRIVATE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
    FILES material/pbr_render_graph_adapter.h
)
target_compile_features(granit_pbr PUBLIC cxx_std_20)
target_include_directories(granit_pbr PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>")
target_link_libraries(granit_pbr PUBLIC granit::material granit::render_graph)
granit_target_compile_warnings(granit_pbr)
granit_target_output_directories(granit_pbr)
set_target_properties(granit_pbr PROPERTIES FOLDER "Modules" POSITION_INDEPENDENT_CODE ON)

add_library(granit_scene STATIC)
add_library(granit::scene ALIAS granit_scene)
target_sources(
  granit_scene
  PRIVATE
    scene/multi_view_submission.cpp
    scene/scene_pbr_adapter.cpp
    scene/scene_submission.cpp
    scene/scene_visibility.cpp
  PRIVATE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
    FILES
      scene/multi_view_submission.h
      scene/scene_pbr_adapter.h
      scene/scene_submission.h
      scene/scene_visibility.h
)
target_compile_features(granit_scene PUBLIC cxx_std_20)
target_include_directories(granit_scene PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>")
target_link_libraries(granit_scene PUBLIC granit::pbr)
granit_target_compile_warnings(granit_scene)
granit_target_output_directories(granit_scene)
set_target_properties(granit_scene PROPERTIES FOLDER "Modules" POSITION_INDEPENDENT_CODE ON)

add_library(granit_lighting STATIC)
add_library(granit::lighting ALIAS granit_lighting)
target_sources(
  granit_lighting
  PRIVATE
    lighting/directional_shadow.cpp
    lighting/ibl_reference.cpp
    lighting/ibl_resources.cpp
    lighting/light_buffers.cpp
    lighting/light_data.cpp
    lighting/lighting_reference.cpp
    lighting/reference_pipeline_graph.cpp
    lighting/shadow_resources.cpp
    lighting/shadow_ibl_resources.cpp
    lighting/shadow_reference.cpp
    lighting/tone_mapping_reference.cpp
    lighting/tone_mapping_pass.cpp
    lighting/tone_mapping_resources.cpp
  PRIVATE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
    FILES
      lighting/directional_shadow.h
      lighting/ibl_reference.h
      lighting/ibl_resources.h
      lighting/light_buffers.h
      lighting/light_data.h
      lighting/lighting_reference.h
      lighting/reference_pipeline_graph.h
      lighting/shadow_resources.h
      lighting/shadow_ibl_resources.h
      lighting/shadow_reference.h
      lighting/tone_mapping_reference.h
      lighting/tone_mapping_pass.h
      lighting/tone_mapping_resources.h
)
target_compile_features(granit_lighting PUBLIC cxx_std_20)
target_include_directories(granit_lighting PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>")
target_link_libraries(granit_lighting PUBLIC granit::scene)
granit_target_compile_warnings(granit_lighting)
granit_target_output_directories(granit_lighting)
set_target_properties(granit_lighting PROPERTIES FOLDER "Modules" POSITION_INDEPENDENT_CODE ON)

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
    "${granit_pipeline_debug_world_vertex}"
    "${granit_pipeline_debug_world_fragment}"
    "${granit_pipeline_debug_world_srgb_fragment}"
    pipeline/embedded_shaders.cpp
    pipeline/embedded_shaders.h
    pipeline/default_ibl_resources.cpp
    pipeline/default_ibl_resources.h
    pipeline/material_api.cpp
    pipeline/material_access.h
    pipeline/mesh_api.cpp
    pipeline/mesh_access.h
    pipeline/pbr_draw_bindings.cpp
    pipeline/pbr_draw_bindings.h
    pipeline/render_pipeline_api.cpp
    pipeline/render_pipeline_metrics.h
    pipeline/scene_access.h
    pipeline/scene_api.cpp
    pipeline/unlit_pass.cpp
    pipeline/unlit_pass.h
    pipeline/canvas_draw_list.h
    pipeline/canvas_draw_list_api.cpp
    pipeline/canvas_geometry_upload.h
    pipeline/canvas_pass.cpp
    pipeline/canvas_pass.h
    pipeline/debug_draw_list_api.cpp
    pipeline/debug_draw_geometry.cpp
    pipeline/debug_draw_geometry.h
    pipeline/text_draw_list_api.cpp
    pipeline/text_atlas_api.cpp
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

set_target_properties(granit_render_graph PROPERTIES EXPORT_NAME detail_render_graph)
set_target_properties(granit_math PROPERTIES EXPORT_NAME detail_math)
set_target_properties(granit_material PROPERTIES EXPORT_NAME detail_material)
set_target_properties(granit_pbr PROPERTIES EXPORT_NAME detail_pbr)
set_target_properties(granit_scene PROPERTIES EXPORT_NAME detail_scene)
set_target_properties(granit_lighting PROPERTIES EXPORT_NAME detail_lighting)

install(
  TARGETS
    granit_render_graph
    granit_math
    granit_material
    granit_pbr
    granit_scene
    granit_lighting
  EXPORT granitRenderPipelineTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT RenderPipeline
)
install(
  TARGETS granit_render_pipeline
  EXPORT granitRenderPipelineTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT RenderPipeline
)
install(
  EXPORT granitRenderPipelineTargets
  NAMESPACE granit::
  DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
  COMPONENT RenderPipeline
)
