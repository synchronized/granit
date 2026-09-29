# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS integration)

if(GRANIT_BUILD_INTEGRATION_SDL3)
  add_executable(granit_integration_sdl3_surface_test sdl3/surface_test.cpp)
  target_link_libraries(
    granit_integration_sdl3_surface_test PRIVATE granit::granit granit::integration_sdl3
  )
  granit_target_compile_warnings(granit_integration_sdl3_surface_test)
  granit_target_output_directories(granit_integration_sdl3_surface_test)
  if(WIN32)
    add_custom_command(
      TARGET granit_integration_sdl3_surface_test
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${GRANIT_SDL3_TARGET}>"
        "$<TARGET_FILE_DIR:granit_integration_sdl3_surface_test>"
      VERBATIM
    )
    add_test(
      NAME granit.integration.sdl3.surface
      COMMAND granit_integration_sdl3_surface_test --smoke-test
    )
    set_tests_properties(granit.integration.sdl3.surface PROPERTIES TIMEOUT 30)
  endif()
endif()

if(TARGET granit::integration_imgui)
  add_executable(granit_integration_imgui_test imgui/renderer_test.cpp)
  target_link_libraries(
    granit_integration_imgui_test
    PRIVATE granit::integration_imgui Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_integration_imgui_test)
  granit_target_output_directories(granit_integration_imgui_test)
  add_test(NAME granit.integration.imgui COMMAND granit_integration_imgui_test)
  set_property(TEST granit.integration.imgui APPEND PROPERTY LABELS gpu)

  if(TARGET granit_example_validation)
    add_executable(
      granit_integration_imgui_visual_test
      imgui/visual_test.cpp
      "${PROJECT_SOURCE_DIR}/examples/tutorials/08_sdl_imgui/resources.cpp"
    )
    target_include_directories(
      granit_integration_imgui_visual_test
      PRIVATE "${PROJECT_SOURCE_DIR}/examples" "${PROJECT_SOURCE_DIR}/examples/common"
              "${granit_imgui_SOURCE_DIR}"
    )
    target_link_libraries(
      granit_integration_imgui_visual_test
      PRIVATE granit::integration_imgui granit_example_validation Catch2::Catch2WithMain
    )
    granit_target_compile_warnings(granit_integration_imgui_visual_test)
    granit_target_output_directories(granit_integration_imgui_visual_test)
    add_test(NAME granit.integration.imgui.visual COMMAND granit_integration_imgui_visual_test)
    set_tests_properties(
      granit.integration.imgui.visual
      PROPERTIES TIMEOUT 60
                 ENVIRONMENT_MODIFICATION
                 "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
    )
    set_property(TEST granit.integration.imgui.visual APPEND PROPERTY LABELS gpu)
  endif()
endif()
if(GRANIT_BUILD_INTEGRATION_SDL3 AND WIN32)
  set_property(TEST granit.integration.sdl3.surface APPEND PROPERTY LABELS gpu platform)
endif()
