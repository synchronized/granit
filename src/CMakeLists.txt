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
    backend/vulkan/upload_context.cpp
    backend/vulkan/upload_context.h
    backend/vulkan/version_check.cpp
    backend/vulkan/vma_implementation.cpp
    core/handle_table.cpp
    core/handle_table.h
    core/lifecycle_validation.cpp
    core/lifecycle_validation.h
    core/resource_validation.cpp
    core/resource_validation.h
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
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/buffer.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/buffer.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/command_recorder.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/command_recorder.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/granit.h"
      "${PROJECT_SOURCE_DIR}/include/granit/granit.hpp"
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
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_STATIC_DEFINE>
)

target_link_libraries(granit PRIVATE ${CMAKE_DL_LIBS})

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
    material/pbr_material_schema.cpp
    material/pbr_reference.cpp
    material/material_template_gpu.cpp
  PUBLIC
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
      material/pbr_material_schema.h
      material/pbr_reference.h
      material/material_template_gpu.h
)
target_compile_features(granit_material PUBLIC cxx_std_20)
target_include_directories(granit_material PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_material PUBLIC granit::granit)
granit_target_compile_warnings(granit_material)
granit_target_output_directories(granit_material)
set_target_properties(
  granit_material
  PROPERTIES
    FOLDER "Modules"
    POSITION_INDEPENDENT_CODE ON
)
