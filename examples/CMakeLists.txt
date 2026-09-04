# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(GRANIT_BUILD_MODEL_VIEWER_EXAMPLE)
  add_subdirectory(common/gltf)
  add_subdirectory(common/model_viewer)
  add_subdirectory(platform/desktop)
  add_subdirectory(platform/web)
endif()

add_executable(granit_version_example version.cpp)
target_link_libraries(granit_version_example PRIVATE granit::granit)
granit_target_compile_warnings(granit_version_example)

add_executable(granit_renderer_example renderer.cpp)
target_link_libraries(granit_renderer_example PRIVATE granit::granit)
granit_target_compile_warnings(granit_renderer_example)

add_executable(granit_offscreen_clear_example offscreen_clear.cpp)
target_link_libraries(granit_offscreen_clear_example PRIVATE granit::granit)
granit_target_compile_warnings(granit_offscreen_clear_example)

add_executable(granit_texture_readback_example texture_readback.cpp)
target_link_libraries(granit_texture_readback_example PRIVATE granit::granit)
granit_target_compile_warnings(granit_texture_readback_example)
if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_test(NAME granit.example.texture_readback COMMAND granit_texture_readback_example)
endif()

add_executable(granit_offscreen_triangle_example offscreen_triangle.cpp)
target_link_libraries(granit_offscreen_triangle_example PRIVATE granit::granit)
target_compile_definitions(
  granit_offscreen_triangle_example
  PRIVATE GRANIT_EXAMPLE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
granit_target_compile_warnings(granit_offscreen_triangle_example)

add_executable(granit_compute_example compute.cpp)
target_link_libraries(granit_compute_example PRIVATE granit::granit)
target_compile_definitions(
  granit_compute_example
  PRIVATE GRANIT_EXAMPLE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
granit_target_compile_warnings(granit_compute_example)

add_executable(granit_material_hot_reload_example material_hot_reload.cpp)
target_link_libraries(granit_material_hot_reload_example PRIVATE granit::material)
target_compile_definitions(
  granit_material_hot_reload_example
  PRIVATE GRANIT_EXAMPLE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
granit_target_compile_warnings(granit_material_hot_reload_example)

add_executable(
  granit_pbr_offscreen_example
  pbr_offscreen.cpp
  pbr_example_support.cpp
  "${PROJECT_SOURCE_DIR}/tests/reference/lighting/ibl_reference.cpp"
  "${PROJECT_SOURCE_DIR}/tests/reference/lighting/lighting_reference.cpp"
  "${PROJECT_SOURCE_DIR}/tests/reference/lighting/tone_mapping_reference.cpp"
  "${PROJECT_SOURCE_DIR}/tests/reference/material/pbr_reference.cpp"
)
target_include_directories(granit_pbr_offscreen_example PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(granit_pbr_offscreen_example PRIVATE granit::lighting)
target_compile_definitions(
  granit_pbr_offscreen_example
  PRIVATE
    GRANIT_PBR_SHADER_DIR="${PROJECT_SOURCE_DIR}/assets/shaders/pbr"
    GRANIT_PIPELINE_SHADER_DIR="${PROJECT_SOURCE_DIR}/src/pipeline/shaders"
)
granit_target_compile_warnings(granit_pbr_offscreen_example)
if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_test(NAME granit.example.pbr_offscreen COMMAND granit_pbr_offscreen_example)
endif()

set(granit_render_pipeline_example_package
    "${CMAKE_CURRENT_BINARY_DIR}/assets/render_pipeline_untextured.grmat")
add_custom_command(
  OUTPUT "${granit_render_pipeline_example_package}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
  COMMAND
    granit_material_tool build
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/render_pipeline_untextured.grmat.json"
    --output "${granit_render_pipeline_example_package}"
  DEPENDS
    granit_material_tool
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/render_pipeline_untextured.grmat.json"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights.vert.spv"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights.vert.wgsl"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights_untextured.frag.spv"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights_untextured.frag.wgsl"
  VERBATIM
)
add_executable(granit_render_pipeline_offscreen_example render_pipeline_offscreen.cpp)
target_link_libraries(granit_render_pipeline_offscreen_example PRIVATE granit::render_pipeline)
target_compile_definitions(
  granit_render_pipeline_offscreen_example
  PRIVATE GRANIT_RENDER_PIPELINE_EXAMPLE_PACKAGE="${granit_render_pipeline_example_package}"
)
add_dependencies(granit_render_pipeline_offscreen_example granit_material_tool)
target_sources(
  granit_render_pipeline_offscreen_example PRIVATE "${granit_render_pipeline_example_package}"
)
granit_target_compile_warnings(granit_render_pipeline_offscreen_example)
if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_test(
    NAME granit.example.render_pipeline_offscreen
    COMMAND granit_render_pipeline_offscreen_example
  )
endif()

add_executable(granit_immediate_ui_adapter_example immediate_ui_adapter.cpp)
target_link_libraries(granit_immediate_ui_adapter_example PRIVATE granit::render_pipeline)
granit_target_compile_warnings(granit_immediate_ui_adapter_example)
if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_test(NAME granit.example.immediate_ui_adapter COMMAND granit_immediate_ui_adapter_example)
endif()

if(GRANIT_BUILD_INTEGRATION_SDL3)
  add_executable(granit_sdl3_window_clear_example sdl3_window_clear.cpp)
  target_link_libraries(
    granit_sdl3_window_clear_example PRIVATE granit::granit granit::integration_sdl3
  )
  if(WIN32)
    add_custom_command(
      TARGET granit_sdl3_window_clear_example
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${GRANIT_SDL3_TARGET}>"
        "$<TARGET_FILE_DIR:granit_sdl3_window_clear_example>"
      VERBATIM
    )
  endif()
  granit_target_compile_warnings(granit_sdl3_window_clear_example)
  if(WIN32 AND GRANIT_BUILD_TESTING AND BUILD_TESTING)
    add_test(
      NAME granit.example.sdl3_window_clear
      COMMAND granit_sdl3_window_clear_example --smoke-test
    )
    set_tests_properties(granit.example.sdl3_window_clear PROPERTIES TIMEOUT 30)
  endif()
endif()

if(GRANIT_BUILD_INTEGRATION_SDL3 AND GRANIT_BUILD_INTEGRATION_IMGUI AND GRANIT_IMGUI_FETCHED)
  add_executable(
    granit_sdl3_imgui_example
    sdl3_imgui.cpp
    "${granit_imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${granit_imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
  )
  target_include_directories(
    granit_sdl3_imgui_example SYSTEM PRIVATE "${granit_imgui_SOURCE_DIR}"
  )
  target_link_libraries(
    granit_sdl3_imgui_example
    PRIVATE granit::integration_sdl3 granit::integration_imgui
  )
  if(WIN32)
    add_custom_command(
      TARGET granit_sdl3_imgui_example
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${GRANIT_SDL3_TARGET}>"
        "$<TARGET_FILE_DIR:granit_sdl3_imgui_example>"
      VERBATIM
    )
  endif()
  granit_target_compile_warnings(granit_sdl3_imgui_example)
  if(WIN32 AND GRANIT_BUILD_TESTING AND BUILD_TESTING)
    add_test(
      NAME granit.example.sdl3_imgui
      COMMAND granit_sdl3_imgui_example --smoke-test
    )
    set_tests_properties(granit.example.sdl3_imgui PROPERTIES TIMEOUT 30)
  endif()
endif()

if(WIN32)
  add_executable(granit_render_pipeline_window_example render_pipeline_window.cpp)
  target_link_libraries(
    granit_render_pipeline_window_example PRIVATE granit::render_pipeline user32
  )
  target_compile_definitions(
    granit_render_pipeline_window_example
    PRIVATE GRANIT_RENDER_PIPELINE_EXAMPLE_PACKAGE="${granit_render_pipeline_example_package}"
  )
  add_dependencies(granit_render_pipeline_window_example granit_material_tool)
  target_sources(
    granit_render_pipeline_window_example PRIVATE "${granit_render_pipeline_example_package}"
  )
  granit_target_compile_warnings(granit_render_pipeline_window_example)
  if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
    add_test(
      NAME granit.example.render_pipeline_window
      COMMAND granit_render_pipeline_window_example --smoke-test
    )
    set_tests_properties(granit.example.render_pipeline_window PROPERTIES TIMEOUT 30)
    set_tests_properties(
      granit.example.render_pipeline_window
      PROPERTIES FAIL_REGULAR_EXPRESSION "\\[granit\\]\\[vulkan\\]"
    )
  endif()

  add_executable(granit_window_clear_example window_clear.cpp)
  target_link_libraries(granit_window_clear_example PRIVATE granit::granit granit::window)
  granit_target_compile_warnings(granit_window_clear_example)
  if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
    add_test(
      NAME granit.example.window_clear
      COMMAND granit_window_clear_example --smoke-test
    )
    set_tests_properties(granit.example.window_clear PROPERTIES TIMEOUT 30)
  endif()

  add_executable(granit_window_triangle_example window_triangle.cpp)
  target_link_libraries(granit_window_triangle_example PRIVATE granit::granit user32)
  target_compile_definitions(
    granit_window_triangle_example
    PRIVATE GRANIT_EXAMPLE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
  )
  granit_target_compile_warnings(granit_window_triangle_example)

  add_executable(granit_window_hdr_example window_hdr.cpp pbr_example_support.cpp)
  target_link_libraries(granit_window_hdr_example PRIVATE granit::lighting user32)
  target_compile_definitions(
    granit_window_hdr_example
    PRIVATE
      GRANIT_PBR_SHADER_DIR="${PROJECT_SOURCE_DIR}/assets/shaders/pbr"
      GRANIT_PIPELINE_SHADER_DIR="${PROJECT_SOURCE_DIR}/src/pipeline/shaders"
  )
  granit_target_compile_warnings(granit_window_hdr_example)
endif()

if(GRANIT_HAS_XCB)
  add_executable(granit_xcb_window_clear_example xcb_window_clear.cpp)
  target_include_directories(
    granit_xcb_window_clear_example SYSTEM PRIVATE "${GRANIT_XCB_INCLUDE_DIR}"
  )
  target_link_libraries(
    granit_xcb_window_clear_example PRIVATE granit::granit "${GRANIT_XCB_LIBRARY}"
  )
  granit_target_compile_warnings(granit_xcb_window_clear_example)
  if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
    add_test(
      NAME granit.example.xcb_window_clear
      COMMAND granit_xcb_window_clear_example --smoke-test
    )
    set_tests_properties(granit.example.xcb_window_clear PROPERTIES TIMEOUT 30)
  endif()
endif()

if(GRANIT_HAS_WAYLAND_SHELL)
  set(granit_example_wayland_protocol_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/wayland")
  set(
    granit_example_xdg_shell_header
    "${granit_example_wayland_protocol_dir}/xdg-shell-client-protocol.h"
  )
  set(
    granit_example_xdg_shell_source
    "${granit_example_wayland_protocol_dir}/xdg-shell-protocol.c"
  )
  add_custom_command(
    OUTPUT "${granit_example_xdg_shell_header}" "${granit_example_xdg_shell_source}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_example_wayland_protocol_dir}"
    COMMAND
      "${GRANIT_WAYLAND_SCANNER}" client-header "${GRANIT_XDG_SHELL_PROTOCOL}"
      "${granit_example_xdg_shell_header}"
    COMMAND
      "${GRANIT_WAYLAND_SCANNER}" private-code "${GRANIT_XDG_SHELL_PROTOCOL}"
      "${granit_example_xdg_shell_source}"
    DEPENDS "${GRANIT_XDG_SHELL_PROTOCOL}"
    VERBATIM
  )
  add_library(
    granit_example_xdg_shell_protocol OBJECT
    "${granit_example_xdg_shell_header}"
    "${granit_example_xdg_shell_source}"
  )
  set_target_properties(
    granit_example_xdg_shell_protocol PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES
  )
  target_include_directories(
    granit_example_xdg_shell_protocol
    SYSTEM PRIVATE "${GRANIT_WAYLAND_INCLUDE_DIR}" "${granit_example_wayland_protocol_dir}"
  )

  add_executable(
    granit_wayland_window_clear_example
    wayland_window_clear.cpp
    $<TARGET_OBJECTS:granit_example_xdg_shell_protocol>
  )
  add_dependencies(granit_wayland_window_clear_example granit_example_xdg_shell_protocol)
  target_include_directories(
    granit_wayland_window_clear_example
    SYSTEM PRIVATE "${GRANIT_WAYLAND_INCLUDE_DIR}" "${granit_example_wayland_protocol_dir}"
  )
  target_link_libraries(
    granit_wayland_window_clear_example PRIVATE granit::granit "${GRANIT_WAYLAND_LIBRARY}"
  )
  granit_target_compile_warnings(granit_wayland_window_clear_example)
  if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
    add_test(
      NAME granit.example.wayland_window_clear
      COMMAND granit_wayland_window_clear_example --smoke-test
    )
    set_tests_properties(granit.example.wayland_window_clear PROPERTIES TIMEOUT 30)
  endif()
endif()

set(
  granit_example_targets
  granit_version_example
  granit_renderer_example
  granit_offscreen_clear_example
  granit_texture_readback_example
  granit_offscreen_triangle_example
  granit_compute_example
  granit_material_hot_reload_example
  granit_pbr_offscreen_example
  granit_render_pipeline_offscreen_example
  granit_immediate_ui_adapter_example
)
if(GRANIT_BUILD_INTEGRATION_SDL3)
  list(APPEND granit_example_targets granit_sdl3_window_clear_example)
endif()
if(GRANIT_BUILD_INTEGRATION_SDL3 AND GRANIT_BUILD_INTEGRATION_IMGUI AND GRANIT_IMGUI_FETCHED)
  list(APPEND granit_example_targets granit_sdl3_imgui_example)
endif()
if(WIN32)
  list(
    APPEND granit_example_targets
    granit_window_clear_example
    granit_window_triangle_example
    granit_window_hdr_example
    granit_render_pipeline_window_example
  )
endif()
if(GRANIT_HAS_XCB)
  list(APPEND granit_example_targets granit_xcb_window_clear_example)
endif()
if(GRANIT_HAS_WAYLAND_SHELL)
  list(APPEND granit_example_targets granit_wayland_window_clear_example)
endif()
foreach(target IN LISTS granit_example_targets)
  granit_target_output_directories(${target})
endforeach()
