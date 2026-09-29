# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Model Viewer 私有 Core、工具和验收目标；由 Sample 目标编排包含。

add_library(
  granit_sample_model_viewer_core STATIC
  app/model_load_operation.cpp
  app/model_load_operation.h
  model/performance_history.cpp
  model/performance_history.h
  model/viewer_document.cpp
  model/viewer_document.h
  model/viewer_state.cpp
  model/viewer_state.h
  rendering/environment_ktx2.cpp
  rendering/environment_ktx2.h
  rendering/pipeline_prepare.cpp
  rendering/pipeline_prepare.h
  rendering/presentation_recovery.h
  rendering/render_dispatcher.cpp
  rendering/render_dispatcher.h
  rendering/render_execution.cpp
  rendering/render_execution.h
  rendering/viewer_renderer.cpp
  rendering/viewer_renderer.h
)
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
    tests/offscreen_acceptance.cpp
  )
  target_link_libraries(
    granit_sample_model_viewer_offscreen_acceptance
    PRIVATE granit_sample_model_viewer_core granit_example_validation
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
    granit_sample_model_viewer_core_test
    app/model_load_operation_test.cpp
    model/performance_history_test.cpp
    model/viewer_document_test.cpp
    model/viewer_state_test.cpp
    rendering/environment_ktx2_test.cpp
    rendering/presentation_recovery_test.cpp
    rendering/render_execution_test.cpp
  )
  target_link_libraries(
    granit_sample_model_viewer_core_test
    PRIVATE granit_sample_model_viewer_core Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_sample_model_viewer_core_test)
  add_test(
    NAME granit.sample.model_viewer_core
    COMMAND granit_sample_model_viewer_core_test
  )
  if(WIN32)
    set_tests_properties(
      granit.sample.model_viewer_core
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::render_pipeline>"
    )
  endif()
endif()

if(TARGET granit::integration_imgui)
  add_library(
    granit_sample_model_viewer_imgui STATIC
    rendering/frame_builder.cpp
    rendering/frame_builder.h
    ui/texture_previews.cpp
    ui/texture_previews.h
    ui/viewer_panels.cpp
    ui/viewer_panels.h
    ui/viewer_ui.cpp
    ui/viewer_ui.h
  )
  target_compile_features(granit_sample_model_viewer_imgui PUBLIC cxx_std_20)
  target_include_directories(
    granit_sample_model_viewer_imgui
    PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples"
  )
  target_link_libraries(
    granit_sample_model_viewer_imgui
    PUBLIC granit_sample_model_viewer_core
           granit_example_imgui
           granit::integration_imgui
  )
  set_target_properties(
    granit_sample_model_viewer_imgui PROPERTIES FOLDER "Examples/Samples"
  )
  granit_target_compile_warnings(granit_sample_model_viewer_imgui)

  add_library(
    granit_sample_model_viewer_app STATIC
    app/model_viewer_app.cpp
    app/model_viewer_app.h
  )
  target_compile_features(granit_sample_model_viewer_app PUBLIC cxx_std_20)
  target_include_directories(
    granit_sample_model_viewer_app
    PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples"
           "${PROJECT_SOURCE_DIR}/examples/common"
  )
  target_include_directories(
    granit_sample_model_viewer_app SYSTEM PRIVATE "${granit_imgui_SOURCE_DIR}"
  )
  target_link_libraries(
    granit_sample_model_viewer_app
    PUBLIC granit_sample_model_viewer_imgui granit_example_application
  )
  set_target_properties(
    granit_sample_model_viewer_app PROPERTIES FOLDER "Examples/Samples"
  )
  granit_target_compile_warnings(granit_sample_model_viewer_app)

  if(GRANIT_HAS_NATIVE_WINDOW AND GRANIT_TESTING_ENABLED)
    add_executable(
      granit_sample_model_viewer_imgui_test
      ui/viewer_panels_test.cpp
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
