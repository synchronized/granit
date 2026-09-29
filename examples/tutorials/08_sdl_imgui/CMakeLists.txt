# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(EMSCRIPTEN)
  add_executable(granit_tutorial_08_sdl_imgui web_main.cpp resources.cpp)
  target_link_libraries(
    granit_tutorial_08_sdl_imgui
    PRIVATE granit::integration_sdl3 granit::integration_imgui
            ${GRANIT_IMGUI_BACKEND_SDL3_TARGET}
  )
  granit_target_webgpu(granit_tutorial_08_sdl_imgui)
  granit_target_web_page(
    granit_tutorial_08_sdl_imgui "${CMAKE_CURRENT_LIST_DIR}/web_shell.html"
  )
else()
  add_executable(granit_tutorial_08_sdl_imgui main.cpp resources.cpp)
  target_link_libraries(
    granit_tutorial_08_sdl_imgui
    PRIVATE granit_example_sdl granit::integration_sdl3 granit::integration_imgui
  )
  if(WIN32)
    add_custom_command(
      TARGET granit_tutorial_08_sdl_imgui
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${GRANIT_SDL3_TARGET}>"
        "$<TARGET_FILE_DIR:granit_tutorial_08_sdl_imgui>"
      VERBATIM
    )
  endif()
  granit_target_output_directories(granit_tutorial_08_sdl_imgui)
endif()

target_compile_features(granit_tutorial_08_sdl_imgui PRIVATE cxx_std_20)
target_include_directories(
  granit_tutorial_08_sdl_imgui
  PRIVATE "${PROJECT_SOURCE_DIR}/examples" "${PROJECT_SOURCE_DIR}/examples/common"
          "${granit_imgui_SOURCE_DIR}"
)
granit_target_compile_warnings(granit_tutorial_08_sdl_imgui)
set_target_properties(granit_tutorial_08_sdl_imgui PROPERTIES FOLDER "Examples/Tutorials")

if(WIN32 AND GRANIT_TESTING_ENABLED)
  add_test(
    NAME granit.tutorial.08_sdl_imgui
    COMMAND granit_tutorial_08_sdl_imgui --smoke-test
  )
  set_tests_properties(
    granit.tutorial.08_sdl_imgui
    PROPERTIES TIMEOUT 30
               FAIL_REGULAR_EXPRESSION "销毁时仍有|ImGui Draw Data 转换失败"
               LABELS "tutorial;smoke;gpu;integration;platform"
  )
endif()
