# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Model Viewer 私有 Core、工具和验收目标；由 Sample 目标编排包含。

add_library(
  granit_sample_model_viewer_core STATIC
  application_core.cpp
  application_core.h
  environment_ktx2.cpp
  environment_ktx2.h
  frame_executor.cpp
  frame_executor.h
  model_loading_session.cpp
  model_loading_session.h
  orbit_camera.cpp
  orbit_camera.h
  performance_history.cpp
  performance_history.h
  viewer_state.cpp
  viewer_state.h
  viewer_input.h
  viewer_input_accumulator.cpp
  viewer_input_accumulator.h
)
add_library(granit_sample_model_viewer_support ALIAS granit_sample_model_viewer_core)
target_compile_features(granit_sample_model_viewer_core PUBLIC cxx_std_20)
target_include_directories(
  granit_sample_model_viewer_core
  PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples"
         "${PROJECT_SOURCE_DIR}/examples/common"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
  granit_sample_model_viewer_core
  PUBLIC granit_example_model_scene granit_example_imgui_canvas
)
set_target_properties(granit_sample_model_viewer_core PROPERTIES FOLDER "Examples/Samples")
granit_target_compile_warnings(granit_sample_model_viewer_core)

if(NOT CMAKE_CROSSCOMPILING)
  add_executable(
    granit_sample_model_viewer_offscreen_acceptance
    offscreen_acceptance.cpp
  )
  target_link_libraries(
    granit_sample_model_viewer_offscreen_acceptance
    PRIVATE granit_sample_model_viewer_support granit_example_validation
  )
  set_target_properties(
    granit_sample_model_viewer_offscreen_acceptance
    PROPERTIES FOLDER "Examples/Samples/Acceptance"
  )
  granit_target_output_directories(granit_sample_model_viewer_offscreen_acceptance)
  granit_target_compile_warnings(granit_sample_model_viewer_offscreen_acceptance)
endif()

if(GRANIT_HAS_NATIVE_WINDOW AND GRANIT_TESTING_ENABLED)
  add_executable(
    granit_sample_model_viewer_support_test
    application_core_test.cpp
    environment_ktx2_test.cpp
    frame_executor_test.cpp
    gpu_scene_test.cpp
    model_loading_session_test.cpp
    orbit_camera_test.cpp
    performance_history_test.cpp
    viewer_state_test.cpp
    viewer_input_accumulator_test.cpp
  )
  target_link_libraries(
    granit_sample_model_viewer_support_test
    PRIVATE granit_sample_model_viewer_support Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_sample_model_viewer_support_test)
  add_test(
    NAME granit.sample.model_viewer_support
    COMMAND granit_sample_model_viewer_support_test
  )
  if(WIN32)
    set_tests_properties(
      granit.sample.model_viewer_support
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::render_pipeline>"
    )
  endif()
endif()

if(TARGET granit::integration_imgui)
  add_library(
    granit_sample_model_viewer_imgui STATIC
    viewer_panels.cpp
    viewer_panels.h
  )
  target_compile_features(granit_sample_model_viewer_imgui PUBLIC cxx_std_20)
  target_include_directories(
    granit_sample_model_viewer_imgui
    PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples"
  )
  target_link_libraries(
    granit_sample_model_viewer_imgui
    PUBLIC granit_sample_model_viewer_support
           granit_example_imgui
           granit::integration_imgui
  )
  set_target_properties(
    granit_sample_model_viewer_imgui PROPERTIES FOLDER "Examples/Samples"
  )
  granit_target_compile_warnings(granit_sample_model_viewer_imgui)

  if(GRANIT_HAS_NATIVE_WINDOW AND GRANIT_TESTING_ENABLED)
    add_executable(
      granit_sample_model_viewer_imgui_test
      viewer_panels_test.cpp
    )
    target_link_libraries(
      granit_sample_model_viewer_imgui_test
      PRIVATE granit_sample_model_viewer_imgui Catch2::Catch2WithMain
    )
    granit_target_compile_warnings(granit_sample_model_viewer_imgui_test)
    add_test(
      NAME granit.sample.model_viewer_imgui
      COMMAND granit_sample_model_viewer_imgui_test
    )
    if(WIN32)
      set_tests_properties(
        granit.sample.model_viewer_imgui
        PROPERTIES ENVIRONMENT_MODIFICATION
                   "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::integration_imgui>"
      )
    endif()
  endif()
endif()
