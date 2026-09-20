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

if(GRANIT_BUILD_INTEGRATION_IMGUI)
  add_executable(granit_integration_imgui_test imgui/renderer_test.cpp)
  target_link_libraries(
    granit_integration_imgui_test
    PRIVATE granit::integration_imgui Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_integration_imgui_test)
  granit_target_output_directories(granit_integration_imgui_test)
  add_test(NAME granit.integration.imgui COMMAND granit_integration_imgui_test)
endif()
if(GRANIT_BUILD_INTEGRATION_SDL3 AND WIN32)
  set_property(TEST granit.integration.sdl3.surface APPEND PROPERTY LABELS gpu platform)
endif()
