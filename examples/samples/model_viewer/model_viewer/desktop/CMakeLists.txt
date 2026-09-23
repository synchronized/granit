# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Model Viewer 的 SDL3/Vulkan 桌面平台壳层。

add_library(
  granit_sample_model_viewer_desktop_shell STATIC
  desktop_options.cpp
  desktop_options.h
  presentation_policy.h
)

target_sources(granit_sample_model_viewer_desktop_shell PRIVATE sdl3_input.cpp sdl3_input.h)
target_link_libraries(granit_sample_model_viewer_desktop_shell PUBLIC "${GRANIT_SDL3_TARGET}")

add_executable(
  granit_sample_model_viewer
  main.cpp
  "${granit_imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
)
target_include_directories(
  granit_sample_model_viewer
  PRIVATE "${PROJECT_SOURCE_DIR}/examples" "${PROJECT_SOURCE_DIR}/examples/common"
)
target_include_directories(
  granit_sample_model_viewer SYSTEM PRIVATE "${granit_imgui_SOURCE_DIR}"
)
target_link_libraries(
  granit_sample_model_viewer
  PRIVATE granit_sample_model_viewer_desktop_shell granit_sample_model_viewer_imgui
          granit::integration_sdl3
)
if(WIN32)
  add_custom_command(
    TARGET granit_sample_model_viewer
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${GRANIT_SDL3_TARGET}>"
      "$<TARGET_FILE_DIR:granit_sample_model_viewer>"
    VERBATIM
  )
endif()
granit_target_compile_warnings(granit_sample_model_viewer)
granit_target_output_directories(granit_sample_model_viewer)

target_compile_features(granit_sample_model_viewer_desktop_shell PUBLIC cxx_std_20)
target_include_directories(
  granit_sample_model_viewer_desktop_shell
  PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer"
)
target_link_libraries(
  granit_sample_model_viewer_desktop_shell PUBLIC granit_sample_model_viewer_core
)
set_target_properties(
  granit_sample_model_viewer_desktop_shell PROPERTIES FOLDER "Examples/Samples"
)
granit_target_compile_warnings(granit_sample_model_viewer_desktop_shell)

if(GRANIT_TESTING_ENABLED)
  add_executable(
    granit_sample_model_viewer_desktop_shell_test
    desktop_options_test.cpp
    presentation_policy_test.cpp
  )
  if(GRANIT_BUILD_INTEGRATION_SDL3)
    target_sources(granit_sample_model_viewer_desktop_shell_test PRIVATE sdl3_input_test.cpp)
  endif()
  target_link_libraries(
    granit_sample_model_viewer_desktop_shell_test
    PRIVATE granit_sample_model_viewer_desktop_shell Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_sample_model_viewer_desktop_shell_test)
  add_test(
    NAME granit.sample.model_viewer_desktop_shell
    COMMAND granit_sample_model_viewer_desktop_shell_test
  )
endif()
