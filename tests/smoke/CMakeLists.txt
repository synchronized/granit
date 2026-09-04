# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(granit_smoke_asset_dir "${PROJECT_SOURCE_DIR}/tests/fixtures/smoke")

function(granit_add_smoke_target target source)
  add_executable(${target} ${source})
  granit_target_compile_warnings(${target})
  granit_target_output_directories(${target})
endfunction()

granit_add_smoke_target(granit_version_smoke version.cpp)
target_link_libraries(granit_version_smoke PRIVATE granit::granit)
add_test(NAME granit.smoke.version COMMAND granit_version_smoke)

granit_add_smoke_target(granit_renderer_smoke renderer.cpp)
target_link_libraries(granit_renderer_smoke PRIVATE granit::granit)
add_test(NAME granit.smoke.renderer COMMAND granit_renderer_smoke)

granit_add_smoke_target(granit_offscreen_clear_smoke offscreen_clear.cpp)
target_link_libraries(granit_offscreen_clear_smoke PRIVATE granit::granit)
add_test(NAME granit.smoke.offscreen_clear COMMAND granit_offscreen_clear_smoke)

granit_add_smoke_target(granit_texture_readback_smoke texture_readback.cpp)
target_link_libraries(granit_texture_readback_smoke PRIVATE granit::granit)
add_test(NAME granit.smoke.texture_readback COMMAND granit_texture_readback_smoke)

granit_add_smoke_target(granit_offscreen_triangle_smoke offscreen_triangle.cpp)
target_link_libraries(granit_offscreen_triangle_smoke PRIVATE granit::granit)
target_compile_definitions(
  granit_offscreen_triangle_smoke
  PRIVATE GRANIT_SMOKE_ASSET_DIR="${granit_smoke_asset_dir}"
)
add_test(NAME granit.smoke.offscreen_triangle COMMAND granit_offscreen_triangle_smoke)

granit_add_smoke_target(granit_compute_smoke compute.cpp)
target_link_libraries(granit_compute_smoke PRIVATE granit::granit)
target_compile_definitions(
  granit_compute_smoke PRIVATE GRANIT_SMOKE_ASSET_DIR="${granit_smoke_asset_dir}"
)
add_test(NAME granit.smoke.compute COMMAND granit_compute_smoke)

granit_add_smoke_target(granit_material_hot_reload_smoke material_hot_reload.cpp)
target_link_libraries(granit_material_hot_reload_smoke PRIVATE granit::material)
target_compile_definitions(
  granit_material_hot_reload_smoke
  PRIVATE GRANIT_SMOKE_ASSET_DIR="${granit_smoke_asset_dir}"
)
add_test(NAME granit.smoke.material_hot_reload COMMAND granit_material_hot_reload_smoke)

granit_add_smoke_target(granit_pbr_offscreen_smoke pbr_offscreen.cpp)
target_sources(
  granit_pbr_offscreen_smoke
  PRIVATE
    pbr_test_support.cpp
    "${PROJECT_SOURCE_DIR}/tests/reference/lighting/ibl_reference.cpp"
    "${PROJECT_SOURCE_DIR}/tests/reference/lighting/lighting_reference.cpp"
    "${PROJECT_SOURCE_DIR}/tests/reference/lighting/tone_mapping_reference.cpp"
    "${PROJECT_SOURCE_DIR}/tests/reference/material/pbr_reference.cpp"
)
target_include_directories(granit_pbr_offscreen_smoke PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(granit_pbr_offscreen_smoke PRIVATE granit::lighting)
target_compile_definitions(
  granit_pbr_offscreen_smoke
  PRIVATE
    GRANIT_PBR_SHADER_DIR="${PROJECT_SOURCE_DIR}/assets/shaders/pbr"
    GRANIT_PIPELINE_SHADER_DIR="${PROJECT_SOURCE_DIR}/src/pipeline/shaders"
)
add_test(NAME granit.smoke.pbr_offscreen COMMAND granit_pbr_offscreen_smoke)

set(
  granit_render_pipeline_smoke_package
  "${CMAKE_CURRENT_BINARY_DIR}/assets/render_pipeline_untextured.grmat"
)
add_custom_command(
  OUTPUT "${granit_render_pipeline_smoke_package}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
  COMMAND
    granit_material_tool build
    "${granit_smoke_asset_dir}/render_pipeline_untextured.grmat.json"
    --output "${granit_render_pipeline_smoke_package}"
  DEPENDS
    granit_material_tool
    "${granit_smoke_asset_dir}/render_pipeline_untextured.grmat.json"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights.vert.spv"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights.vert.wgsl"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights_untextured.frag.spv"
    "${PROJECT_SOURCE_DIR}/assets/shaders/pbr/pbr_shadow_ibl_lights_untextured.frag.wgsl"
  VERBATIM
)

granit_add_smoke_target(granit_render_pipeline_offscreen_smoke render_pipeline_offscreen.cpp)
target_link_libraries(granit_render_pipeline_offscreen_smoke PRIVATE granit::render_pipeline)
target_compile_definitions(
  granit_render_pipeline_offscreen_smoke
  PRIVATE GRANIT_RENDER_PIPELINE_EXAMPLE_PACKAGE="${granit_render_pipeline_smoke_package}"
)
add_dependencies(granit_render_pipeline_offscreen_smoke granit_material_tool)
target_sources(
  granit_render_pipeline_offscreen_smoke PRIVATE "${granit_render_pipeline_smoke_package}"
)
add_test(
  NAME granit.smoke.render_pipeline_offscreen
  COMMAND granit_render_pipeline_offscreen_smoke
)

granit_add_smoke_target(granit_immediate_ui_adapter_smoke immediate_ui_adapter.cpp)
target_link_libraries(granit_immediate_ui_adapter_smoke PRIVATE granit::render_pipeline)
add_test(NAME granit.smoke.immediate_ui_adapter COMMAND granit_immediate_ui_adapter_smoke)

if(GRANIT_BUILD_INTEGRATION_SDL3)
  granit_add_smoke_target(granit_sdl3_window_clear_smoke sdl3_window_clear.cpp)
  target_link_libraries(
    granit_sdl3_window_clear_smoke PRIVATE granit::granit granit::integration_sdl3
  )
  if(WIN32)
    add_custom_command(
      TARGET granit_sdl3_window_clear_smoke
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${GRANIT_SDL3_TARGET}>"
        "$<TARGET_FILE_DIR:granit_sdl3_window_clear_smoke>"
      VERBATIM
    )
    add_test(
      NAME granit.smoke.sdl3_window_clear
      COMMAND granit_sdl3_window_clear_smoke --smoke-test
    )
    set_tests_properties(granit.smoke.sdl3_window_clear PROPERTIES TIMEOUT 30)
  endif()
endif()

if(WIN32)
  granit_add_smoke_target(granit_render_pipeline_window_smoke render_pipeline_window.cpp)
  target_link_libraries(granit_render_pipeline_window_smoke PRIVATE granit::render_pipeline user32)
  target_compile_definitions(
    granit_render_pipeline_window_smoke
    PRIVATE GRANIT_RENDER_PIPELINE_EXAMPLE_PACKAGE="${granit_render_pipeline_smoke_package}"
  )
  add_dependencies(granit_render_pipeline_window_smoke granit_material_tool)
  target_sources(
    granit_render_pipeline_window_smoke PRIVATE "${granit_render_pipeline_smoke_package}"
  )
  add_test(
    NAME granit.smoke.render_pipeline_window
    COMMAND granit_render_pipeline_window_smoke --smoke-test
  )
  set_tests_properties(
    granit.smoke.render_pipeline_window
    PROPERTIES TIMEOUT 30 FAIL_REGULAR_EXPRESSION "\\[granit\\]\\[vulkan\\]"
  )

  granit_add_smoke_target(granit_window_clear_smoke window_clear.cpp)
  target_link_libraries(granit_window_clear_smoke PRIVATE granit::granit granit::window)
  add_test(NAME granit.smoke.window_clear COMMAND granit_window_clear_smoke --smoke-test)
  set_tests_properties(granit.smoke.window_clear PROPERTIES TIMEOUT 30)

  granit_add_smoke_target(granit_window_triangle_smoke window_triangle.cpp)
  target_link_libraries(granit_window_triangle_smoke PRIVATE granit::granit user32)
  target_compile_definitions(
    granit_window_triangle_smoke PRIVATE GRANIT_SMOKE_ASSET_DIR="${granit_smoke_asset_dir}"
  )
  add_test(NAME granit.smoke.window_triangle COMMAND granit_window_triangle_smoke --smoke-test)
  set_tests_properties(granit.smoke.window_triangle PROPERTIES TIMEOUT 30)

  granit_add_smoke_target(granit_window_hdr_smoke window_hdr.cpp)
  target_sources(granit_window_hdr_smoke PRIVATE pbr_test_support.cpp)
  target_link_libraries(granit_window_hdr_smoke PRIVATE granit::lighting user32)
  target_compile_definitions(
    granit_window_hdr_smoke
    PRIVATE
      GRANIT_PBR_SHADER_DIR="${PROJECT_SOURCE_DIR}/assets/shaders/pbr"
      GRANIT_PIPELINE_SHADER_DIR="${PROJECT_SOURCE_DIR}/src/pipeline/shaders"
  )
  add_test(NAME granit.smoke.window_hdr COMMAND granit_window_hdr_smoke --smoke-test)
  set_tests_properties(granit.smoke.window_hdr PROPERTIES TIMEOUT 30)
endif()

if(GRANIT_HAS_XCB)
  granit_add_smoke_target(granit_xcb_window_clear_smoke xcb_window_clear.cpp)
  target_include_directories(
    granit_xcb_window_clear_smoke SYSTEM PRIVATE "${GRANIT_XCB_INCLUDE_DIR}"
  )
  target_link_libraries(
    granit_xcb_window_clear_smoke PRIVATE granit::granit "${GRANIT_XCB_LIBRARY}"
  )
  add_test(NAME granit.smoke.xcb_window_clear COMMAND granit_xcb_window_clear_smoke --smoke-test)
  set_tests_properties(granit.smoke.xcb_window_clear PROPERTIES TIMEOUT 30)
endif()

if(GRANIT_HAS_WAYLAND_SHELL)
  set(granit_smoke_wayland_protocol_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/wayland")
  set(
    granit_smoke_xdg_shell_header
    "${granit_smoke_wayland_protocol_dir}/xdg-shell-client-protocol.h"
  )
  set(
    granit_smoke_xdg_shell_source
    "${granit_smoke_wayland_protocol_dir}/xdg-shell-protocol.c"
  )
  add_custom_command(
    OUTPUT "${granit_smoke_xdg_shell_header}" "${granit_smoke_xdg_shell_source}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_smoke_wayland_protocol_dir}"
    COMMAND
      "${GRANIT_WAYLAND_SCANNER}" client-header "${GRANIT_XDG_SHELL_PROTOCOL}"
      "${granit_smoke_xdg_shell_header}"
    COMMAND
      "${GRANIT_WAYLAND_SCANNER}" private-code "${GRANIT_XDG_SHELL_PROTOCOL}"
      "${granit_smoke_xdg_shell_source}"
    DEPENDS "${GRANIT_XDG_SHELL_PROTOCOL}"
    VERBATIM
  )
  add_library(
    granit_smoke_xdg_shell_protocol OBJECT
    "${granit_smoke_xdg_shell_header}"
    "${granit_smoke_xdg_shell_source}"
  )
  set_target_properties(
    granit_smoke_xdg_shell_protocol PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES
  )
  target_include_directories(
    granit_smoke_xdg_shell_protocol
    SYSTEM PRIVATE "${GRANIT_WAYLAND_INCLUDE_DIR}" "${granit_smoke_wayland_protocol_dir}"
  )

  granit_add_smoke_target(granit_wayland_window_clear_smoke wayland_window_clear.cpp)
  target_sources(
    granit_wayland_window_clear_smoke PRIVATE $<TARGET_OBJECTS:granit_smoke_xdg_shell_protocol>
  )
  add_dependencies(granit_wayland_window_clear_smoke granit_smoke_xdg_shell_protocol)
  target_include_directories(
    granit_wayland_window_clear_smoke
    SYSTEM PRIVATE "${GRANIT_WAYLAND_INCLUDE_DIR}" "${granit_smoke_wayland_protocol_dir}"
  )
  target_link_libraries(
    granit_wayland_window_clear_smoke PRIVATE granit::granit "${GRANIT_WAYLAND_LIBRARY}"
  )
  add_test(
    NAME granit.smoke.wayland_window_clear
    COMMAND granit_wayland_window_clear_smoke --smoke-test
  )
  set_tests_properties(granit.smoke.wayland_window_clear PROPERTIES TIMEOUT 30)
endif()
