# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_test(
  NAME granit.backend.native_boundaries
  COMMAND
    "${CMAKE_COMMAND}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${PROJECT_SOURCE_DIR}/cmake/check_backend_native_boundaries.cmake"
)

add_test(
  NAME granit.documentation
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${PROJECT_SOURCE_DIR}/cmake/check_documentation.cmake"
)

add_test(
  NAME granit.release_version
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DGRANIT_RELEASE_TAG=v${PROJECT_VERSION}"
    -P "${PROJECT_SOURCE_DIR}/cmake/check_release_version.cmake"
)
add_test(
  NAME granit.release_version_mismatch
  COMMAND
    "${CMAKE_COMMAND}"
    "-DGRANIT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -DGRANIT_RELEASE_TAG=v999.0.0
    -P "${PROJECT_SOURCE_DIR}/cmake/check_release_version.cmake"
)
set_tests_properties(granit.release_version_mismatch PROPERTIES WILL_FAIL TRUE)

add_executable(granit_c_version_test core/version_c_test.c)
target_link_libraries(granit_c_version_test PRIVATE granit::granit Unity::Unity)
set_target_properties(granit_c_version_test PROPERTIES LINKER_LANGUAGE CXX)
granit_target_compile_warnings(granit_c_version_test)
add_test(NAME granit.c_version COMMAND granit_c_version_test)

add_library(
  granit_c_header_test
  OBJECT
    abi/layout_h.c
    headers/diagnostic_h.c
    headers/command_recorder_h.c
    headers/frame_context_h.c
    headers/math_types_h.c
    headers/input_h.c
    headers/timestamp_query_h.c
    headers/pipeline_h.c
    headers/buffer_h.c
    headers/renderer_h.c
    headers/render_target_h.c
    headers/resource_types_h.c
    headers/sampler_h.c
    headers/shader_h.c
    headers/result_h.c
    headers/surface_h.c
    headers/swapchain_h.c
    headers/texture_h.c
    headers/types_h.c
    headers/upload_batch_h.c
    headers/window_h.c
    headers/pipeline_material_h.c
    headers/pipeline_mesh_h.c
    headers/pipeline_render_pipeline_h.c
    headers/pipeline_scene_h.c
      headers/pipeline_canvas_draw_list_h.c
      headers/pipeline_debug_draw_list_h.c
      headers/pipeline_text_draw_list_h.c
      headers/pipeline_text_atlas_h.c
)
target_compile_features(granit_c_header_test PRIVATE c_std_11)
target_link_libraries(
  granit_c_header_test PRIVATE granit::granit granit::render_pipeline granit::window granit::input
)
granit_target_compile_warnings(granit_c_header_test)

add_executable(granit_cpp_version_test core/version_cpp_test.cpp)
target_link_libraries(
  granit_cpp_version_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_cpp_version_test)
add_test(NAME granit.cpp_version COMMAND granit_cpp_version_test)

if(BUILD_SHARED_LIBS)
  add_executable(granit_abi_exports_test abi/export_symbols_test.cpp)
  target_compile_features(granit_abi_exports_test PRIVATE cxx_std_20)
  target_compile_definitions(
    granit_abi_exports_test
    PRIVATE
      GRANIT_ABI_CORE_LIBRARY_PATH="$<TARGET_FILE:granit>"
      GRANIT_ABI_RENDER_PIPELINE_LIBRARY_PATH="$<TARGET_FILE:granit_render_pipeline>"
      GRANIT_ABI_WINDOW_LIBRARY_PATH="$<TARGET_FILE:granit_window>"
      GRANIT_ABI_INPUT_LIBRARY_PATH="$<TARGET_FILE:granit_input>"
      GRANIT_ABI_CORE_SYMBOLS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/abi/snapshots/0.4.0/core_symbols.txt"
      GRANIT_ABI_RENDER_PIPELINE_SYMBOLS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/abi/snapshots/0.1.0/render_pipeline_symbols.txt"
      GRANIT_ABI_WINDOW_SYMBOLS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/abi/snapshots/0.1.0/window_symbols.txt"
      GRANIT_ABI_INPUT_SYMBOLS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/abi/snapshots/0.1.0/input_symbols.txt"
  )
  target_include_directories(
    granit_abi_exports_test
    PRIVATE "$<TARGET_PROPERTY:granit,INTERFACE_INCLUDE_DIRECTORIES>"
  )
  target_link_libraries(
    granit_abi_exports_test
    PRIVATE Catch2::Catch2WithMain ${CMAKE_DL_LIBS}
  )
  add_dependencies(
    granit_abi_exports_test granit granit_render_pipeline granit_window granit_input
  )
  granit_target_compile_warnings(granit_abi_exports_test)
  granit_target_output_directories(granit_abi_exports_test)
  add_test(NAME granit.abi.exports COMMAND granit_abi_exports_test)

  if(MSVC)
    set(granit_abi_export_tool "${CMAKE_DUMPBIN}")
    set(granit_abi_export_tool_kind dumpbin)
  elseif(UNIX)
    set(granit_abi_export_tool "${CMAKE_NM}")
    set(granit_abi_export_tool_kind nm)
  endif()

  if(granit_abi_export_tool)
    function(granit_add_abi_export_snapshot_test component target snapshot)
      add_test(
        NAME "granit.abi.exports.${component}"
        COMMAND
          "${CMAKE_COMMAND}"
          "-DGRANIT_ABI_COMPONENT=${component}"
          "-DGRANIT_ABI_LIBRARY=$<TARGET_FILE:${target}>"
          "-DGRANIT_ABI_SNAPSHOT=${CMAKE_CURRENT_SOURCE_DIR}/abi/snapshots/0.1.0/${snapshot}"
          "-DGRANIT_ABI_EXPORT_TOOL=${granit_abi_export_tool}"
          "-DGRANIT_ABI_EXPORT_TOOL_KIND=${granit_abi_export_tool_kind}"
          "-DGRANIT_ABI_PRIVATE_EXPORTS=${ARGN}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/abi/check_exported_symbols.cmake"
      )
    endfunction()

    add_test(
      NAME granit.abi.exports.core
      COMMAND
        "${CMAKE_COMMAND}"
        "-DGRANIT_ABI_COMPONENT=core"
        "-DGRANIT_ABI_LIBRARY=$<TARGET_FILE:granit>"
        "-DGRANIT_ABI_SNAPSHOT=${CMAKE_CURRENT_SOURCE_DIR}/abi/snapshots/0.4.0/core_symbols.txt"
        "-DGRANIT_ABI_EXPORT_TOOL=${granit_abi_export_tool}"
        "-DGRANIT_ABI_EXPORT_TOOL_KIND=${granit_abi_export_tool_kind}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/abi/check_exported_symbols.cmake"
    )
    granit_add_abi_export_snapshot_test(
      render_pipeline granit_render_pipeline render_pipeline_symbols.txt
      "${CMAKE_CURRENT_SOURCE_DIR}/abi/snapshots/0.1.0/render_pipeline_private_symbols.txt"
    )
    granit_add_abi_export_snapshot_test(
      window granit_window window_symbols.txt
      "${CMAKE_CURRENT_SOURCE_DIR}/abi/snapshots/0.1.0/window_private_symbols.txt"
    )
    granit_add_abi_export_snapshot_test(input granit_input input_symbols.txt)
  endif()
endif()

add_executable(
  granit_handle_table_test
  core/handle_table_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/handle_table.cpp"
)
target_include_directories(granit_handle_table_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_handle_table_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_handle_table_test)
add_test(NAME granit.handle_table COMMAND granit_handle_table_test)

add_executable(
  granit_resource_validation_test
  core/resource_validation_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/resource_validation.cpp"
)
target_include_directories(granit_resource_validation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_resource_validation_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_resource_validation_test)
add_test(NAME granit.resource_validation COMMAND granit_resource_validation_test)

add_executable(granit_backend_capabilities_test backend/capabilities_test.cpp)
target_include_directories(granit_backend_capabilities_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_backend_capabilities_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_backend_capabilities_test)
add_test(NAME granit.backend_capabilities COMMAND granit_backend_capabilities_test)

add_executable(granit_backend_resources_test backend/resources_test.cpp)
target_include_directories(granit_backend_resources_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_backend_resources_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_backend_resources_test)
add_test(NAME granit.backend_resources COMMAND granit_backend_resources_test)

add_library(granit_backend_plugin_c_header_test OBJECT backend/plugin_api_h.c)
target_compile_features(granit_backend_plugin_c_header_test PRIVATE c_std_11)
target_include_directories(
  granit_backend_plugin_c_header_test
  PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
granit_target_compile_warnings(granit_backend_plugin_c_header_test)

add_library(
  granit_fake_backend_plugin SHARED
  "${PROJECT_SOURCE_DIR}/src/backend/webgpu/plugin.cpp"
  backend/webgpu_mock.cpp
)
add_library(granit_incompatible_backend_plugin SHARED backend/incompatible_backend_plugin.cpp)
target_compile_features(granit_fake_backend_plugin PRIVATE cxx_std_20)
target_compile_definitions(
  granit_fake_backend_plugin
  PRIVATE GRANIT_WEBGPU_FORCE_FALLBACK_ADAPTER GRANIT_WEBGPU_DEFER_INITIALIZATION_TEST
          GRANIT_WEBGPU_CANVAS_SURFACE_TEST GRANIT_WEBGPU_NATIVE_SURFACE_TEST
)
target_compile_features(granit_incompatible_backend_plugin PRIVATE cxx_std_20)
set_target_properties(
  granit_fake_backend_plugin granit_incompatible_backend_plugin
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    FOLDER "Tests/WebGPU"
)
target_include_directories(
  granit_fake_backend_plugin
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support/webgpu"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
)
target_include_directories(
  granit_incompatible_backend_plugin
  PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
granit_target_output_directories(granit_fake_backend_plugin)
granit_target_output_directories(granit_incompatible_backend_plugin)
granit_target_compile_warnings(granit_fake_backend_plugin)
granit_target_compile_warnings(granit_incompatible_backend_plugin)

add_executable(
  granit_backend_plugin_loader_test
  backend/plugin_loader_test.cpp
  backend/webgpu_presentation_adapter_test.cpp
  backend/webgpu_renderer_state_test.cpp
  platform/shared_library_test.cpp
  $<TARGET_OBJECTS:granit_backend_plugin>
)
target_include_directories(
  granit_backend_plugin_loader_test
  PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
target_compile_definitions(
  granit_backend_plugin_loader_test
  PRIVATE
    GRANIT_FAKE_BACKEND_PLUGIN_PATH="$<TARGET_FILE:granit_fake_backend_plugin>"
    GRANIT_INCOMPATIBLE_BACKEND_PLUGIN_PATH="$<TARGET_FILE:granit_incompatible_backend_plugin>"
    GRANIT_MISSING_BACKEND_PLUGIN_PATH="${CMAKE_CURRENT_BINARY_DIR}/missing-plugin"
)
target_link_libraries(
  granit_backend_plugin_loader_test PRIVATE Catch2::Catch2WithMain ${CMAKE_DL_LIBS}
)
add_dependencies(
  granit_backend_plugin_loader_test granit_fake_backend_plugin
  granit_incompatible_backend_plugin
)
granit_target_compile_warnings(granit_backend_plugin_loader_test)
granit_target_output_directories(granit_backend_plugin_loader_test)
add_test(NAME granit.backend.plugin_loader COMMAND granit_backend_plugin_loader_test)

if(GRANIT_BUILD_BACKEND_WEBGPU)
  add_executable(
    granit_webgpu_plugin_smoke
    backend/webgpu_plugin_smoke.cpp
    $<TARGET_OBJECTS:granit_backend_plugin>
    $<TARGET_OBJECTS:granit_shader_asset>
  )
  target_compile_features(granit_webgpu_plugin_smoke PRIVATE cxx_std_20)
  target_include_directories(
    granit_webgpu_plugin_smoke
    PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tools"
  )
  target_compile_definitions(
    granit_webgpu_plugin_smoke
    PRIVATE GRANIT_WEBGPU_PLUGIN_PATH="$<TARGET_FILE:granit_backend_webgpu>"
  )
  target_link_libraries(granit_webgpu_plugin_smoke PRIVATE ${CMAKE_DL_LIBS})
  add_dependencies(granit_webgpu_plugin_smoke granit_backend_webgpu)
  granit_target_compile_warnings(granit_webgpu_plugin_smoke)
  granit_target_output_directories(granit_webgpu_plugin_smoke)
  if(NOT GRANIT_S10C_VERTEX_SHADER_ASSET OR NOT GRANIT_S10C_FRAGMENT_SHADER_ASSET)
    message(FATAL_ERROR "S-10C6 smoke test 需要设置 Vertex 和 Fragment Shader 资产")
  endif()
  add_test(
    NAME granit.backend.webgpu_plugin_smoke
    COMMAND granit_webgpu_plugin_smoke "${GRANIT_S10C_VERTEX_SHADER_ASSET}"
  )

  add_executable(
    granit_shader_asset_vulkan_smoke
    backend/shader_asset_vulkan_smoke.cpp
    $<TARGET_OBJECTS:granit_shader_asset>
  )
  target_compile_features(granit_shader_asset_vulkan_smoke PRIVATE cxx_std_20)
  target_include_directories(
    granit_shader_asset_vulkan_smoke PRIVATE "${PROJECT_SOURCE_DIR}/tools"
  )
  target_link_libraries(granit_shader_asset_vulkan_smoke PRIVATE granit::granit)
  granit_target_compile_warnings(granit_shader_asset_vulkan_smoke)
  granit_target_output_directories(granit_shader_asset_vulkan_smoke)
  add_test(
    NAME granit.backend.shader_asset_vulkan_smoke
    COMMAND
      granit_shader_asset_vulkan_smoke
      "${GRANIT_S10C_VERTEX_SHADER_ASSET}"
      "${GRANIT_S10C_FRAGMENT_SHADER_ASSET}"
  )
endif()

add_executable(
  granit_lifecycle_validation_test
  core/diagnostic_sink_test.cpp
  core/lifecycle_validation_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/diagnostic_sink.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/lifecycle_validation.cpp"
)
target_include_directories(
  granit_lifecycle_validation_test
  PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(granit_lifecycle_validation_test PRIVATE Catch2::Catch2WithMain)
target_compile_options(
  granit_lifecycle_validation_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_lifecycle_validation_test)
add_test(NAME granit.lifecycle_validation COMMAND granit_lifecycle_validation_test)

add_executable(
  granit_retirement_queue_test
  core/retirement_queue_test.cpp
  "${PROJECT_SOURCE_DIR}/src/core/retirement_queue.cpp"
)
target_include_directories(granit_retirement_queue_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_retirement_queue_test PRIVATE Catch2::Catch2WithMain)
target_compile_options(
  granit_retirement_queue_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_retirement_queue_test)
add_test(NAME granit.retirement_queue COMMAND granit_retirement_queue_test)

add_executable(granit_device_status_test core/device_status_test.cpp)
target_include_directories(
  granit_device_status_test
  PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(granit_device_status_test PRIVATE Catch2::Catch2WithMain)
target_compile_options(
  granit_device_status_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_device_status_test)
add_test(NAME granit.device_status COMMAND granit_device_status_test)

add_executable(
  granit_render_graph_test
  render_graph/graph_compiler_test.cpp
  render_graph/serial_graph_test.cpp
)
target_include_directories(granit_render_graph_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_render_graph_test
  PRIVATE granit::render_graph Catch2::Catch2WithMain $<$<PLATFORM_ID:Windows>:user32>
)
target_compile_options(
  granit_render_graph_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
granit_target_compile_warnings(granit_render_graph_test)
add_test(NAME granit.render_graph COMMAND granit_render_graph_test)

add_executable(granit_math_test math/math_test.cpp)
target_include_directories(granit_math_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_math_test PRIVATE granit::math Catch2::Catch2WithMain)
target_compile_options(granit_math_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
granit_target_compile_warnings(granit_math_test)
add_test(NAME granit.math COMMAND granit_math_test)

add_executable(granit_pbr_test material/pbr_render_graph_adapter_test.cpp)
target_include_directories(granit_pbr_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_pbr_test PRIVATE granit::pbr Catch2::Catch2WithMain)
target_compile_options(granit_pbr_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
granit_target_compile_warnings(granit_pbr_test)
add_test(NAME granit.pbr COMMAND granit_pbr_test)

add_executable(
  granit_scene_test
  scene/multi_view_submission_test.cpp
  scene/scene_pbr_adapter_test.cpp
  scene/scene_submission_test.cpp
  scene/scene_visibility_test.cpp
)
target_include_directories(granit_scene_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_scene_test PRIVATE granit::scene Catch2::Catch2WithMain)
target_compile_options(granit_scene_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
granit_target_compile_warnings(granit_scene_test)
add_test(NAME granit.scene COMMAND granit_scene_test)

add_executable(
  granit_lighting_test
  lighting/directional_shadow_test.cpp
  lighting/ibl_reference_test.cpp
  lighting/ibl_resources_test.cpp
  lighting/light_buffers_test.cpp
  lighting/light_data_test.cpp
  lighting/lighting_reference_test.cpp
  lighting/multi_view_gpu_test.cpp
  lighting/reference_pipeline_graph_test.cpp
  lighting/shadow_resources_test.cpp
  lighting/shadow_ibl_resources_test.cpp
  lighting/shadow_reference_test.cpp
  lighting/tone_mapping_reference_test.cpp
  lighting/tone_mapping_pass_test.cpp
  lighting/tone_mapping_resources_test.cpp
  "${PROJECT_SOURCE_DIR}/examples/pbr_example_support.cpp"
)
target_include_directories(
  granit_lighting_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/examples"
)
target_link_libraries(granit_lighting_test PRIVATE granit::lighting Catch2::Catch2WithMain)
target_compile_options(granit_lighting_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
target_compile_definitions(
  granit_lighting_test
  PRIVATE
    GRANIT_PBR_SHADER_DIR="${PROJECT_SOURCE_DIR}/assets/shaders/pbr"
    GRANIT_PIPELINE_SHADER_DIR="${PROJECT_SOURCE_DIR}/src/pipeline/shaders"
)
granit_target_compile_warnings(granit_lighting_test)
granit_target_output_directories(granit_lighting_test)
add_test(NAME granit.lighting COMMAND granit_lighting_test)

add_executable(
  granit_pipeline_test
  pipeline/material_api_test.cpp
  pipeline/mesh_api_test.cpp
  pipeline/render_pipeline_api_test.cpp
  pipeline/scene_api_test.cpp
  pipeline/unlit_pass_test.cpp
  pipeline/canvas_draw_list_test.cpp
  pipeline/canvas_draw_list_api_test.cpp
  pipeline/canvas_geometry_upload_test.cpp
  pipeline/canvas_pass_test.cpp
  "${PROJECT_SOURCE_DIR}/src/pipeline/dynamic_uniform_arena.cpp"
  pipeline/dynamic_uniform_arena_test.cpp
  pipeline/debug_draw_list_api_test.cpp
  pipeline/debug_draw_geometry_test.cpp
  pipeline/text_draw_list_api_test.cpp
  pipeline/text_atlas_api_test.cpp
)
set(granit_unlit_test_package "${CMAKE_CURRENT_BINARY_DIR}/assets/unlit.grmat")
add_custom_command(
  OUTPUT "${granit_unlit_test_package}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
  COMMAND granit_material_tool build "${PROJECT_SOURCE_DIR}/examples/assets/unlit.grmat.json"
          --output "${granit_unlit_test_package}"
  DEPENDS
    granit_material_tool
    "${PROJECT_SOURCE_DIR}/examples/assets/unlit.grmat.json"
    "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/unlit.vert.spv"
    "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/unlit.vert.wgsl"
    "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/unlit.frag.spv"
    "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/unlit.frag.wgsl"
    "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/unlit_alpha_cutoff.frag.spv"
    "${PROJECT_SOURCE_DIR}/assets/shaders/unlit/unlit_alpha_cutoff.frag.wgsl"
  VERBATIM
)
add_custom_target(granit_unlit_test_package DEPENDS "${granit_unlit_test_package}")
add_dependencies(granit_pipeline_test granit_unlit_test_package)
target_include_directories(granit_pipeline_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_pipeline_test
  PRIVATE
    granit::render_pipeline
    granit::lighting
    granit::material
    granit::granit
    Catch2::Catch2WithMain
)
target_compile_options(granit_pipeline_test PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
target_compile_definitions(
  granit_pipeline_test
  PRIVATE
    GRANIT_PIPELINE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets/shaders/pbr"
    GRANIT_PIPELINE_SHADER_DIR="${PROJECT_SOURCE_DIR}/src/pipeline/shaders"
    GRANIT_UNLIT_TEST_PACKAGE="${granit_unlit_test_package}"
)
granit_target_compile_warnings(granit_pipeline_test)
granit_target_output_directories(granit_pipeline_test)
add_test(NAME granit.pipeline COMMAND granit_pipeline_test)

add_executable(
  granit_material_test
  material/material_gpu_instance_test.cpp
  material/material_hot_reload_test.cpp
  material/material_archive_test.cpp
  material/material_metadata_test.cpp
  material/material_migration_test.cpp
  material/material_package_test.cpp
  material/material_package_archive_test.cpp
  material/pbr_material_schema_test.cpp
  material/pbr_draw_inputs_test.cpp
  material/pbr_reference_test.cpp
  material/material_source_json_test.cpp
  material/material_template_gpu_test.cpp
  "${PROJECT_SOURCE_DIR}/src/material/material_debug_json.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_source_json.cpp"
)
target_include_directories(granit_material_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_material_test PRIVATE granit::material Catch2::Catch2WithMain)
target_compile_options(
  granit_material_test
  PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)
target_compile_definitions(
  granit_material_test
  PRIVATE GRANIT_TEST_ASSET_DIR="${PROJECT_SOURCE_DIR}/tests/assets"
)
granit_target_compile_warnings(granit_material_test)
add_test(NAME granit.material COMMAND granit_material_test)

add_executable(
  granit_renderer_test
  renderer/command_recorder_test.cpp
  renderer/pipeline_test.cpp
  renderer/buffer_test.cpp
  renderer/renderer_test.cpp
  renderer/sampler_test.cpp
  renderer/shader_test.cpp
  renderer/surface_test.cpp
  renderer/swapchain_test.cpp
  renderer/texture_test.cpp
  renderer/upload_batch_test.cpp
)
if(GRANIT_HAS_XCB)
  target_sources(granit_renderer_test PRIVATE renderer/xcb_swapchain_test.cpp)
  target_include_directories(granit_renderer_test SYSTEM PRIVATE "${GRANIT_XCB_INCLUDE_DIR}")
  target_link_libraries(granit_renderer_test PRIVATE "${GRANIT_XCB_LIBRARY}")
endif()
if(GRANIT_HAS_WAYLAND_SHELL)
  target_sources(
    granit_renderer_test
    PRIVATE renderer/wayland_swapchain_test.cpp $<TARGET_OBJECTS:granit_xdg_shell_protocol>
  )
  add_dependencies(granit_renderer_test granit_xdg_shell_protocol)
  target_include_directories(
    granit_renderer_test
    SYSTEM PRIVATE "${GRANIT_WAYLAND_INCLUDE_DIR}" "${granit_wayland_protocol_dir}"
  )
  target_link_libraries(granit_renderer_test PRIVATE "${GRANIT_WAYLAND_LIBRARY}")
endif()
target_link_libraries(
  granit_renderer_test
  PRIVATE
    granit::granit
    Catch2::Catch2WithMain
    $<$<PLATFORM_ID:Windows>:user32>
)
target_include_directories(granit_renderer_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(
  granit_renderer_test
  PRIVATE
    GRANIT_TEST_ASSET_DIR="${PROJECT_SOURCE_DIR}/tests/assets"
    GRANIT_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts"
    GRANIT_FAKE_BACKEND_PLUGIN_PATH="$<TARGET_FILE:granit_fake_backend_plugin>"
)
add_dependencies(granit_renderer_test granit_fake_backend_plugin)
if(GRANIT_BUILD_BACKEND_WEBGPU)
  target_compile_definitions(
    granit_renderer_test
    PRIVATE GRANIT_WEBGPU_BACKEND_PLUGIN_PATH="$<TARGET_FILE:granit_backend_webgpu>"
  )
  add_dependencies(granit_renderer_test granit_backend_webgpu)
endif()
granit_target_compile_warnings(granit_renderer_test)
add_test(NAME granit.renderer COMMAND granit_renderer_test)
add_test(
  NAME granit.renderer.lifecycle_diagnostic
  COMMAND granit_renderer_test "[renderer][lifecycle]"
)
set_tests_properties(
  granit.renderer.lifecycle_diagnostic
  PROPERTIES PASS_REGULAR_EXPRESSION "\\[granit\\]\\[lifecycle\\].*Buffer=1"
)

add_executable(
  granit_vulkan_backend_test
  backend/vulkan_backend_test.cpp
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/device.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/frame_context.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/instance.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/loader.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/memory_allocator.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/physical_device.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/result.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/surface.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/swapchain.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/timestamp_query.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/vma_implementation.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/diagnostic_sink.cpp"
  $<TARGET_OBJECTS:granit_volk>
)
target_include_directories(
  granit_vulkan_backend_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_include_directories(
  granit_vulkan_backend_test
  SYSTEM PRIVATE
    "${GRANIT_VOLK_DIR}"
    "${GRANIT_VMA_DIR}"
    "${GRANIT_VULKAN_HEADERS_DIR}"
)
target_compile_definitions(
  granit_vulkan_backend_test
  PRIVATE
    VK_NO_PROTOTYPES
    VOLK_NAMESPACE
    VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_VULKAN_VERSION=1003000
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
)
target_link_libraries(
  granit_vulkan_backend_test
  PRIVATE granit::granit Catch2::Catch2WithMain ${CMAKE_DL_LIBS}
)
granit_target_compile_warnings(granit_vulkan_backend_test)
add_test(NAME granit.vulkan_backend COMMAND granit_vulkan_backend_test)

add_library(
  granit_cpp_header_test
  OBJECT
    headers/diagnostic_hpp.cpp
    headers/command_recorder_hpp.cpp
    headers/frame_context_hpp.cpp
    headers/math_types_hpp.cpp
    headers/input_hpp.cpp
    headers/timestamp_query_hpp.cpp
    headers/pipeline_hpp.cpp
    headers/buffer_hpp.cpp
    headers/renderer_hpp.cpp
    headers/render_target_hpp.cpp
    headers/resource_types_hpp.cpp
    headers/sampler_hpp.cpp
    headers/shader_hpp.cpp
    headers/result_hpp.cpp
    headers/surface_hpp.cpp
    headers/swapchain_hpp.cpp
    headers/texture_hpp.cpp
    headers/types_hpp.cpp
    headers/upload_batch_hpp.cpp
    headers/pipeline_material_hpp.cpp
    headers/pipeline_mesh_hpp.cpp
    headers/pipeline_render_pipeline_hpp.cpp
    headers/pipeline_scene_hpp.cpp
      headers/pipeline_canvas_draw_list_hpp.cpp
      headers/pipeline_debug_draw_list_hpp.cpp
      headers/pipeline_text_draw_list_hpp.cpp
      headers/pipeline_text_atlas_hpp.cpp
    headers/window_hpp.cpp
)
target_link_libraries(
  granit_cpp_header_test PRIVATE granit::granit granit::render_pipeline granit::window granit::input
)
granit_target_compile_warnings(granit_cpp_header_test)

add_executable(granit_window_test window/window_api_test.cpp)
target_link_libraries(granit_window_test PRIVATE granit::window Catch2::Catch2WithMain)
if(GRANIT_HAS_XCB)
  target_compile_definitions(granit_window_test PRIVATE GRANIT_TEST_HAS_XCB)
  target_include_directories(granit_window_test SYSTEM PRIVATE "${GRANIT_XCB_INCLUDE_DIR}")
  target_link_libraries(granit_window_test PRIVATE "${GRANIT_XCB_LIBRARY}")
endif()
if(GRANIT_HAS_WAYLAND_SHELL)
  target_compile_definitions(granit_window_test PRIVATE GRANIT_TEST_HAS_WAYLAND)
endif()
granit_target_compile_warnings(granit_window_test)
granit_target_output_directories(granit_window_test)
add_test(NAME granit.window COMMAND granit_window_test)

add_executable(
  granit_input_test
  input/input_api_test.cpp
  input/utf8_test.cpp
  $<TARGET_OBJECTS:granit_input_internal>
)
if(GRANIT_HAS_XCB)
  target_sources(granit_input_test PRIVATE input/xcb_input_adapter_test.cpp)
endif()
if(GRANIT_HAS_WAYLAND_INPUT)
  target_sources(granit_input_test PRIVATE input/wayland_input_adapter_test.cpp)
  target_link_libraries(granit_input_test PRIVATE "${GRANIT_XKBCOMMON_LIBRARY}")
endif()
target_include_directories(granit_input_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(granit_input_test PRIVATE granit::input Catch2::Catch2WithMain)
granit_target_compile_warnings(granit_input_test)
granit_target_output_directories(granit_input_test)
add_test(NAME granit.input COMMAND granit_input_test)

add_executable(granit_window_renderer_test window/window_renderer_test.cpp)
target_link_libraries(
  granit_window_renderer_test PRIVATE granit::window granit::granit Catch2::Catch2WithMain
)
if(GRANIT_HAS_XCB)
  target_compile_definitions(granit_window_renderer_test PRIVATE GRANIT_TEST_HAS_XCB)
endif()
if(GRANIT_HAS_WAYLAND_SHELL)
  target_compile_definitions(granit_window_renderer_test PRIVATE GRANIT_TEST_HAS_WAYLAND)
endif()
granit_target_compile_warnings(granit_window_renderer_test)
granit_target_output_directories(granit_window_renderer_test)
add_test(NAME granit.window_renderer COMMAND granit_window_renderer_test)

if(GRANIT_BUILD_INTEGRATION_IMGUI)
  add_executable(granit_integration_imgui_test integrations/imgui/renderer_test.cpp)
  target_link_libraries(
    granit_integration_imgui_test
    PRIVATE granit::integration_imgui Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_integration_imgui_test)
  granit_target_output_directories(granit_integration_imgui_test)
  add_test(NAME granit.integration.imgui COMMAND granit_integration_imgui_test)
endif()

set(
  granit_test_executable_targets
  granit_c_version_test
  granit_cpp_version_test
  granit_handle_table_test
  granit_resource_validation_test
  granit_lifecycle_validation_test
  granit_retirement_queue_test
  granit_device_status_test
  granit_render_graph_test
  granit_pbr_test
  granit_scene_test
  granit_material_test
  granit_renderer_test
  granit_vulkan_backend_test
  granit_pipeline_test
)
foreach(target IN LISTS granit_test_executable_targets)
  granit_target_output_directories(${target})
endforeach()

set_tests_properties(
  granit.c_version granit.cpp_version granit.handle_table granit.resource_validation
  granit.lifecycle_validation granit.retirement_queue granit.device_status granit.renderer
  granit.vulkan_backend
  PROPERTIES ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit>"
)
