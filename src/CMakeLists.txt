# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(granit)
add_library(granit::granit ALIAS granit)
granit_target_output_directories(granit)

add_subdirectory(assets)

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
    $<TARGET_OBJECTS:granit_internal_shader_asset_format>
    backend/contracts/access.h
    backend/contracts/binding.h
    backend/contracts/capabilities.h
    backend/contracts/command.h
    backend/contracts/compute.h
    backend/contracts/diagnostics.h
    backend/contracts/pipeline.h
    backend/contracts/presentation.h
    backend/contracts/queue.h
    backend/contracts/rendering.h
    backend/contracts/retirement.h
    backend/contracts/resource_management.h
    backend/contracts/transfer.h
    backend/contracts/renderer.h
    backend/contracts/resources.h
    backend/contracts/shader.h
    backend/contracts/timestamp.h
    backend/contracts/upload.h
    backend/contracts/callback_lifetime.h
    backend/contracts/lifecycle.h
    core/handle_table.h
    core/async_operation_state.h
    core/diagnostic_sink.h
    core/lifecycle_validation.h
    core/resource_validation.h
    core/texture_format.h
    assets/texture_asset.h
    core/retirement_queue.h
    core/device_status.h
    renderer/renderer_factory.h
    renderer/renderer_registry.h
    renderer/renderer_registry_helpers.h
    renderer/renderer_registry_records.h
    renderer/renderer_validation.h
    renderer/surface_validation.h
    renderer/shader_validation.h
    renderer/swapchain_validation.h
    result.cpp
    version.cpp
    core/handle_table.cpp
    core/async_operation_state.cpp
    core/diagnostic_sink.cpp
    core/lifecycle_validation.cpp
    core/retirement_queue.cpp
    core/resource_validation.cpp
    assets/texture_asset.cpp
    renderer/renderer_api.cpp
    renderer/async_operation_api.cpp
    renderer/pipeline_api.cpp
    renderer/buffer_api.cpp
    renderer/command_recorder_api.cpp
    renderer/frame_context_api.cpp
    renderer/renderer_registry.cpp
    renderer/renderer_registry_async.cpp
    renderer/renderer_registry_buffer.cpp
    renderer/renderer_registry_command.cpp
    renderer/renderer_registry_pipeline.cpp
    renderer/renderer_registry_presentation.cpp
    renderer/renderer_registry_readback.cpp
    renderer/renderer_registry_rendering.cpp
    renderer/renderer_registry_shader_library.cpp
    renderer/renderer_registry_texture.cpp
    renderer/renderer_registry_timestamp.cpp
    renderer/renderer_registry_transfer.cpp
    renderer/renderer_registry_warmup.cpp
    renderer/sampler_api.cpp
    renderer/shader_api.cpp
    renderer/shader_library_api.cpp
    renderer/surface_api.cpp
    renderer/swapchain_api.cpp
    renderer/texture_api.cpp
    renderer/texture_asset_api.cpp
    renderer/timestamp_query_api.cpp
    renderer/upload_batch_api.cpp
    renderer/readback_batch_api.cpp
    renderer/pipeline_warmup_api.cpp
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
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/async_operation.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/async_operation.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/command_recorder.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/command_recorder.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/frame.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/frame_context.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/frame_context.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/granit.h"
      "${PROJECT_SOURCE_DIR}/include/granit/granit.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/math/types.h"
      "${PROJECT_SOURCE_DIR}/include/granit/math/types.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/pipeline.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/pipeline.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/pipeline_warmup.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/pipeline_warmup.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/renderer.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/renderer.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/render_target.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/render_target.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/resource_types.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/resource_types.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/core/result.h"
      "${PROJECT_SOURCE_DIR}/include/granit/core/result.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/core/shader_features.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/sampler.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/sampler.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/shader.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/shader.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/shader_library.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/shader_library.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/surface.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/surface.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/texture.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/texture.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/texture_asset.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/texture_asset.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/timestamp_query.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/timestamp_query.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/upload_batch.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/upload_batch.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/readback_batch.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/readback_batch.hpp"
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
target_compile_definitions(
  granit
  PRIVATE GRANIT_BUILDING_LIBRARY
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_STATIC_DEFINE>
)

if(EMSCRIPTEN)
  add_subdirectory(backend/webgpu)
else()
  add_subdirectory(backend/vulkan)
endif()

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

include(granit_modules)
granit_add_render_graph_module()
granit_add_math_module()
granit_add_material_module()
granit_add_pbr_module()
granit_add_scene_module()
granit_add_lighting_module()
granit_add_render_pipeline_module()

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
install(
  FILES "${PROJECT_SOURCE_DIR}/assets/materials/pbr_standard.grmat"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/granit/assets/materials"
  COMPONENT RenderPipeline
)
install(
  FILES
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_standard.vert.grshader"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_standard.vert.grshader.spv"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_standard.vert.grshader.wgsl"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_standard.frag.grshader"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_standard.frag.grshader.spv"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_standard.frag.grshader.wgsl"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/granit/assets/shaders/pbr"
  COMPONENT RenderPipeline
)
install(
  FILES
    "${PROJECT_SOURCE_DIR}/assets/environments/studio_small_03.grenv"
    "${PROJECT_SOURCE_DIR}/assets/environments/studio_small_03.manifest.json"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/granit/assets/environments"
  COMPONENT RenderPipeline
)

# 原生窗口及其输入适配器尚无浏览器实现，保留真实的平台边界。
if(NOT EMSCRIPTEN)
  if(GRANIT_HAS_WAYLAND_SHELL)
    add_subdirectory(platform/wayland)
  endif()
  add_subdirectory(platform/window)
  add_subdirectory(input)
  if(GRANIT_BUILD_INTEGRATION_SDL3)
    add_subdirectory(integrations/sdl3)
  endif()
  if(GRANIT_BUILD_INTEGRATION_IMGUI)
    add_subdirectory(integrations/imgui)
  endif()
endif()
