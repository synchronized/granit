# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Model Viewer 私有 Core、工具和验收目标；由 Sample 目标编排包含。

add_library(
  granit_sample_model_viewer_core STATIC
  environment_ktx2.cpp
  environment_ktx2.h
  render_execution.cpp
  render_execution.h
  render_dispatcher.cpp
  render_dispatcher.h
  viewer_renderer.cpp
  viewer_renderer.h
  model_load_operation.cpp
  model_load_operation.h
  viewer_document.cpp
  viewer_document.h
  performance_history.cpp
  performance_history.h
  pipeline_prepare.cpp
  pipeline_prepare.h
  presentation_recovery.h
  viewer_state.cpp
  viewer_state.h
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
  PUBLIC granit::window granit_example_camera granit_example_gltf_rendering granit_example_imgui_canvas
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
    environment_ktx2_test.cpp
    render_execution_test.cpp
    model_load_operation_test.cpp
    performance_history_test.cpp
    presentation_recovery_test.cpp
    viewer_document_test.cpp
    viewer_state_test.cpp
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
    viewer_ui.cpp
    viewer_ui.h
    viewer_panels.cpp
    viewer_panels.h
    viewer_texture_previews.cpp
    viewer_texture_previews.h
    viewer_frame_builder.cpp
    viewer_frame_builder.h
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

  add_library(
    granit_sample_model_viewer_application STATIC
    viewer_application.cpp
    viewer_application.h
  )
  target_compile_features(granit_sample_model_viewer_application PUBLIC cxx_std_20)
  target_include_directories(
    granit_sample_model_viewer_application
    PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples"
           "${PROJECT_SOURCE_DIR}/examples/common"
  )
  target_include_directories(
    granit_sample_model_viewer_application SYSTEM PRIVATE "${granit_imgui_SOURCE_DIR}"
  )
  target_link_libraries(
    granit_sample_model_viewer_application
    PUBLIC granit_sample_model_viewer_imgui granit_example_application
  )
  set_target_properties(
    granit_sample_model_viewer_application PROPERTIES FOLDER "Examples/Samples"
  )
  granit_target_compile_warnings(granit_sample_model_viewer_application)

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
