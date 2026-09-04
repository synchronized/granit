# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(
  granit_backend_plugin OBJECT
  backend/plugin/plugin_api.h
  backend/plugin/plugin_loader.cpp
  backend/plugin/plugin_loader.h
  backend/webgpu/command_adapter.cpp
  backend/webgpu/command_adapter.h
  backend/webgpu/presentation_adapter.cpp
  backend/webgpu/presentation_adapter.h
  backend/webgpu/pipeline_adapter.cpp
  backend/webgpu/pipeline_adapter.h
  backend/webgpu/renderer_state.cpp
  backend/webgpu/renderer_state.h
  backend/webgpu/renderer_factory.h
  backend/webgpu/resource_adapter.cpp
  backend/webgpu/resource_adapter.h
  backend/webgpu/shader_adapter.cpp
  backend/webgpu/shader_adapter.h
  platform/shared_library.cpp
  platform/shared_library.h
  $<$<PLATFORM_ID:Windows>:platform/win32/shared_library_backend.cpp>
  $<$<NOT:$<PLATFORM_ID:Windows>>:platform/posix/shared_library_backend.cpp>
)
target_compile_features(granit_backend_plugin PRIVATE cxx_std_20)
target_include_directories(
  granit_backend_plugin PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
granit_target_compile_warnings(granit_backend_plugin)
set_target_properties(
  granit_backend_plugin
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
    VISIBILITY_INLINES_HIDDEN YES
    FOLDER "Backends"
)

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
    $<TARGET_OBJECTS:granit_backend_plugin>
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
    backend/vulkan/resources.cpp
    backend/vulkan/resources.h
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
    backend/contracts/callback_lifetime.h
    backend/contracts/lifecycle.h
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
    renderer/renderer_factory.cpp
    renderer/renderer_factory.h
    renderer/pipeline_api.cpp
    renderer/buffer_api.cpp
    renderer/command_recorder_api.cpp
    renderer/frame_context_api.cpp
    renderer/renderer_registry.cpp
    backend/vulkan/renderer_factory.cpp
    backend/webgpu/renderer_factory.cpp
    renderer/renderer_registry_buffer.cpp
    renderer/renderer_registry_command.cpp
    renderer/renderer_registry.h
    renderer/renderer_registry_helpers.h
    renderer/renderer_registry_pipeline.cpp
    renderer/renderer_registry_presentation.cpp
    renderer/renderer_registry_records.h
    renderer/renderer_registry_rendering.cpp
    renderer/renderer_registry_timestamp.cpp
    renderer/renderer_registry_texture.cpp
    renderer/renderer_registry_transfer.cpp
    backend/vulkan/vulkan_renderer_state.cpp
    backend/vulkan/vulkan_renderer_state.h
    renderer/renderer_validation.h
    renderer/surface_validation.h
    renderer/shader_validation.h
    renderer/swapchain_validation.h
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
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/frame_context.h"
      "${PROJECT_SOURCE_DIR}/include/granit/renderer/frame_context.hpp"
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
  PRIVATE platform/window/window_registry.cpp
          platform/window/window_backend_internal.h
          $<$<PLATFORM_ID:Windows>:platform/win32/window_backend.cpp>
          $<$<BOOL:${GRANIT_HAS_XCB}>:platform/xcb/window_backend.cpp>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:platform/wayland/window_backend.cpp>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:$<TARGET_OBJECTS:granit_xdg_shell_protocol>>
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
  granit_window
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_compile_definitions(
  granit_window
  PRIVATE GRANIT_WINDOW_BUILDING_LIBRARY
          $<$<BOOL:${GRANIT_HAS_XCB}>:GRANIT_WINDOW_HAS_XCB>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:GRANIT_WINDOW_HAS_WAYLAND>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_INPUT}>:GRANIT_WINDOW_HAS_WAYLAND_INPUT>
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_WINDOW_STATIC_DEFINE>
)
target_include_directories(
  granit_window SYSTEM PRIVATE $<$<BOOL:${GRANIT_HAS_XCB}>:${GRANIT_XCB_INCLUDE_DIR}>
                               $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:${GRANIT_WAYLAND_INCLUDE_DIR}>
                               $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:${granit_wayland_protocol_dir}>
)
target_link_libraries(
  granit_window
  PRIVATE $<$<BOOL:${GRANIT_HAS_XCB}>:${GRANIT_XCB_LIBRARY}>
          $<$<BOOL:${GRANIT_HAS_WAYLAND_SHELL}>:${GRANIT_WAYLAND_LIBRARY}>
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

add_library(granit_input_internal OBJECT input/utf8.cpp)
target_compile_features(granit_input_internal PRIVATE cxx_std_20)
target_include_directories(
  granit_input_internal PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
granit_target_compile_warnings(granit_input_internal)
set_target_properties(
  granit_input_internal
  PROPERTIES FOLDER "Modules/Internal" POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES
)
if(GRANIT_HAS_XCB)
  target_sources(granit_input_internal PRIVATE platform/xcb/input_adapter.cpp)
endif()
if(GRANIT_HAS_WAYLAND_INPUT)
  target_sources(granit_input_internal PRIVATE platform/wayland/input_adapter.cpp)
  target_include_directories(
    granit_input_internal SYSTEM PRIVATE "${GRANIT_XKBCOMMON_INCLUDE_DIR}"
  )
endif()

add_library(granit_input)
add_library(granit::input ALIAS granit_input)
target_sources(
  granit_input
  PRIVATE input/input_api.cpp platform/input_adapter.cpp platform/input_adapter.h
          $<TARGET_OBJECTS:granit_input_internal>
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/granit/input.h"
      "${PROJECT_SOURCE_DIR}/include/granit/input.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/input/export.h"
      "${PROJECT_SOURCE_DIR}/include/granit/input/input.h"
      "${PROJECT_SOURCE_DIR}/include/granit/input/input.hpp"
)
if(GRANIT_HAS_XCB)
  target_compile_definitions(granit_input PRIVATE GRANIT_INPUT_HAS_XCB)
endif()
if(GRANIT_HAS_WAYLAND_INPUT)
  target_compile_definitions(granit_input PRIVATE GRANIT_INPUT_HAS_WAYLAND)
  target_link_libraries(granit_input PRIVATE "${GRANIT_XKBCOMMON_LIBRARY}")
endif()
if(WIN32)
  target_sources(granit_input PRIVATE platform/win32/input_adapter.cpp)
endif()
target_compile_features(granit_input PUBLIC cxx_std_20)
target_include_directories(
  granit_input
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_compile_definitions(
  granit_input
  PRIVATE GRANIT_INPUT_BUILDING_LIBRARY
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_INPUT_STATIC_DEFINE>
)
target_link_libraries(granit_input PUBLIC granit::window)
granit_target_compile_warnings(granit_input)
granit_target_output_directories(granit_input)
set_target_properties(
  granit_input
  PROPERTIES EXPORT_NAME input FOLDER "Modules" CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES
)
install(
  TARGETS granit_input
  EXPORT granitInputTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Input
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Input
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Input
  FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT Input
)
install(
  EXPORT granitInputTargets
  NAMESPACE granit::
  DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
  COMPONENT Input
)

if(GRANIT_BUILD_INTEGRATION_SDL3)
  add_library(granit_integration_sdl3)
  add_library(granit::integration_sdl3 ALIAS granit_integration_sdl3)
  target_sources(
    granit_integration_sdl3
    PRIVATE integrations/sdl3/surface.cpp
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
      FILES
        "${PROJECT_SOURCE_DIR}/include/granit/integrations/sdl3/export.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/integrations/sdl3/surface.hpp"
  )
  target_compile_features(granit_integration_sdl3 PUBLIC cxx_std_20)
  target_include_directories(
    granit_integration_sdl3 PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  )
  target_link_libraries(granit_integration_sdl3 PUBLIC granit::granit "${GRANIT_SDL3_TARGET}")
  target_compile_definitions(
    granit_integration_sdl3
    PRIVATE GRANIT_INTEGRATION_SDL3_BUILDING_LIBRARY
    PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_INTEGRATION_SDL3_STATIC_DEFINE>
  )
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_path(GRANIT_X11_XCB_INCLUDE_DIR X11/Xlib-xcb.h)
    find_library(GRANIT_X11_XCB_LIBRARY NAMES X11-xcb)
    if(GRANIT_X11_XCB_INCLUDE_DIR AND GRANIT_X11_XCB_LIBRARY)
      target_compile_definitions(granit_integration_sdl3 PRIVATE GRANIT_INTEGRATION_SDL3_HAS_X11)
      target_include_directories(
        granit_integration_sdl3 SYSTEM PRIVATE "${GRANIT_X11_XCB_INCLUDE_DIR}"
      )
      target_link_libraries(granit_integration_sdl3 PRIVATE "${GRANIT_X11_XCB_LIBRARY}")
    else()
      message(STATUS "Granit SDL3 Integration: X11 路径禁用，未找到 X11-xcb")
    endif()
  endif()
  granit_target_compile_warnings(granit_integration_sdl3)
  granit_target_output_directories(granit_integration_sdl3)
  set_target_properties(
    granit_integration_sdl3
    PROPERTIES
      EXPORT_NAME integration_sdl3
      FOLDER "Integrations"
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES
  )
  if(NOT GRANIT_SDL3_FETCHED)
    install(
      TARGETS granit_integration_sdl3
      EXPORT granitIntegrationSDL3Targets
      ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      COMPONENT IntegrationSDL3
      LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      COMPONENT IntegrationSDL3
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
      COMPONENT IntegrationSDL3
      FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
      COMPONENT IntegrationSDL3
    )
    install(
      EXPORT granitIntegrationSDL3Targets
      NAMESPACE granit::
      DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
      COMPONENT IntegrationSDL3
    )
  endif()
endif()

if(GRANIT_BUILD_INTEGRATION_IMGUI)
  add_library(granit_integration_imgui)
  add_library(granit::integration_imgui ALIAS granit_integration_imgui)
  target_sources(
    granit_integration_imgui
    PRIVATE integrations/imgui/renderer.cpp
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
      FILES
        "${PROJECT_SOURCE_DIR}/include/granit/integrations/imgui/export.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/integrations/imgui/renderer.hpp"
  )
  target_compile_features(granit_integration_imgui PUBLIC cxx_std_20)
  target_include_directories(
    granit_integration_imgui PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  )
  target_link_libraries(
    granit_integration_imgui PUBLIC granit::render_pipeline "${GRANIT_IMGUI_TARGET}"
  )
  target_compile_definitions(
    granit_integration_imgui
    PRIVATE GRANIT_INTEGRATION_IMGUI_BUILDING_LIBRARY
    PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_INTEGRATION_IMGUI_STATIC_DEFINE>
  )
  granit_target_compile_warnings(granit_integration_imgui)
  granit_target_output_directories(granit_integration_imgui)
  set_target_properties(
    granit_integration_imgui
    PROPERTIES
      EXPORT_NAME integration_imgui
      FOLDER "Integrations"
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES
  )
  if(NOT GRANIT_IMGUI_FETCHED)
    install(
      TARGETS granit_integration_imgui
      EXPORT granitIntegrationImGuiTargets
      ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      COMPONENT IntegrationImGui
      LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      COMPONENT IntegrationImGui
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
      COMPONENT IntegrationImGui
      FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
      COMPONENT IntegrationImGui
    )
    install(
      EXPORT granitIntegrationImGuiTargets
      NAMESPACE granit::
      DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
      COMPONENT IntegrationImGui
    )
  endif()
endif()

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
