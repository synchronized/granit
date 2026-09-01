# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(
  granit STATIC
  "${PROJECT_SOURCE_DIR}/src/renderer/buffer_api.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/command_recorder_api.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/pipeline_api.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/renderer_api.cpp"
  renderer_factory.cpp
  "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/renderer_factory.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_buffer.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_command.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_pipeline.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_presentation.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_rendering.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_texture.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_timestamp.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_transfer.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/sampler_api.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/shader_api.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/surface_api.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/swapchain_api.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/texture_api.cpp"
  "${PROJECT_SOURCE_DIR}/src/renderer/upload_batch_api.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/plugin_loader.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/plugin.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/command_adapter.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/presentation_adapter.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/pipeline_adapter.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/resource_adapter.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/renderer_state.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/shader_adapter.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/handle_table.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/diagnostic_sink.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/lifecycle_validation.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/retirement_queue.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/resource_validation.cpp"
  "${PROJECT_SOURCE_DIR}/src/platform/shared_library.cpp"
  "${PROJECT_SOURCE_DIR}/src/platform/emscripten/shared_library_backend.cpp"
)
add_library(granit::granit ALIAS granit)
target_compile_features(granit PUBLIC cxx_std_20)
target_include_directories(
  granit
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_compile_definitions(granit PUBLIC GRANIT_STATIC_DEFINE)
target_compile_options(granit PRIVATE "--use-port=emdawnwebgpu:cpp_bindings=false")
target_link_options(granit INTERFACE "--use-port=emdawnwebgpu:cpp_bindings=false")
granit_target_compile_warnings(granit)

# Web 与桌面复用同一 Math 模块目标，为完整 Model Viewer Core 提供一致依赖。
include(granit_modules)
granit_add_material_module()

# 浏览器模型查看器复用桌面端同一套 glTF Loader 与锁定依赖，不维护 Web 专用副本。
add_subdirectory(
  "${PROJECT_SOURCE_DIR}/examples/common/gltf" "${CMAKE_CURRENT_BINARY_DIR}/gltf"
)

add_library(
  granit_web_model_viewer_platform STATIC
  model_viewer_fetch.cpp
  model_viewer_fetch.h
  "${PROJECT_SOURCE_DIR}/examples/platform/web/asset_request.cpp"
  "${PROJECT_SOURCE_DIR}/examples/platform/web/asset_request.h"
  "${PROJECT_SOURCE_DIR}/examples/platform/web/resource_bundle.cpp"
  "${PROJECT_SOURCE_DIR}/examples/platform/web/resource_bundle.h"
  "${PROJECT_SOURCE_DIR}/examples/platform/web/resource_fetch_batch.cpp"
  "${PROJECT_SOURCE_DIR}/examples/platform/web/resource_fetch_batch.h"
  "${PROJECT_SOURCE_DIR}/examples/platform/web/web_input.cpp"
  "${PROJECT_SOURCE_DIR}/examples/platform/web/web_input.h"
)
target_compile_features(granit_web_model_viewer_platform PUBLIC cxx_std_20)
target_include_directories(
  granit_web_model_viewer_platform
  PUBLIC "${PROJECT_SOURCE_DIR}/examples/platform/web"
         "${PROJECT_SOURCE_DIR}/examples/common"
)
target_link_libraries(
  granit_web_model_viewer_platform PUBLIC granit::granit granit_example_gltf_support
)
target_compile_options(
  granit_web_model_viewer_platform PRIVATE "--use-port=emdawnwebgpu:cpp_bindings=false"
)
granit_target_compile_warnings(granit_web_model_viewer_platform)

add_executable(granit_web_platform_smoke platform_smoke.cpp)
target_compile_features(granit_web_platform_smoke PRIVATE cxx_std_20)
target_link_libraries(granit_web_platform_smoke PRIVATE granit::granit)
target_include_directories(
  granit_web_platform_smoke PRIVATE "${PROJECT_SOURCE_DIR}/include"
                                    "${PROJECT_SOURCE_DIR}/tests"
)
target_link_options(
  granit_web_platform_smoke
  PRIVATE
    "--use-port=emdawnwebgpu:cpp_bindings=false"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets/s10d_startup.txt@/assets/s10d_startup.txt"
    "--preload-file=${PROJECT_SOURCE_DIR}/tests/assets/dynamic_uniform.vert.wgsl@/assets/dynamic_uniform.vert.wgsl"
    "--preload-file=${PROJECT_SOURCE_DIR}/tests/assets/dynamic_uniform.frag.wgsl@/assets/dynamic_uniform.frag.wgsl"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_EXIT_RUNTIME=1"
    "-sFETCH=1"
    "$<$<CONFIG:Debug>:-sASSERTIONS=1>"
    "$<$<CONFIG:Release>:--closure=1>"
)
target_link_libraries(granit_web_platform_smoke PRIVATE granit_web_model_viewer_platform)
set_target_properties(
  granit_web_platform_smoke
  PROPERTIES
    OUTPUT_NAME granit_webgpu_fixture_example
    SUFFIX ".html"
    FOLDER "Examples/Web"
)
granit_target_compile_warnings(granit_web_platform_smoke)

add_custom_command(
  TARGET granit_web_platform_smoke
  POST_BUILD
  COMMAND
    "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/model_viewer_fixture.gltf"
    "$<TARGET_FILE_DIR:granit_web_platform_smoke>/model_viewer_fixture.gltf"
  COMMAND
    "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/model_viewer_fixture.bin"
    "$<TARGET_FILE_DIR:granit_web_platform_smoke>/model_viewer_fixture.bin"
  VERBATIM
)
