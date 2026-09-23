# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Model Viewer 私有 Core、工具和验收目标；由示例自身的目标编排包含。

add_library(
  granit_model_viewer_core STATIC
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/application_core.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/application_core.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/environment_ktx2.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/environment_ktx2.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/frame_executor.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/frame_executor.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/orbit_camera.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/orbit_camera.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/performance_history.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/performance_history.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/viewer_state.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/viewer_state.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/viewer_input.h"
)
add_library(granit_example_model_viewer_support ALIAS granit_model_viewer_core)
target_compile_features(granit_model_viewer_core PUBLIC cxx_std_20)
target_include_directories(
  granit_model_viewer_core
  PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples" "${PROJECT_SOURCE_DIR}/examples/common"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
  granit_model_viewer_core
  PUBLIC granit_example_model_scene granit_example_imgui_canvas
)
set_target_properties(granit_model_viewer_core PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_model_viewer_core)

if(NOT CMAKE_CROSSCOMPILING)
  add_executable(
    granit_model_viewer_offscreen_acceptance
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/offscreen_acceptance.cpp"
  )
  target_link_libraries(
    granit_model_viewer_offscreen_acceptance
    PRIVATE granit_example_model_viewer_support granit_example_validation
  )
  set_target_properties(
    granit_model_viewer_offscreen_acceptance PROPERTIES FOLDER "Examples/Acceptance"
  )
  granit_target_output_directories(granit_model_viewer_offscreen_acceptance)
  granit_target_compile_warnings(granit_model_viewer_offscreen_acceptance)
endif()

if(GRANIT_HAS_NATIVE_WINDOW AND GRANIT_TESTING_ENABLED)
  add_executable(
    granit_example_model_viewer_support_test
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/application_core_test.cpp"
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/environment_ktx2_test.cpp"
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/frame_executor_test.cpp"
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/gpu_scene_test.cpp"
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/orbit_camera_test.cpp"
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/performance_history_test.cpp"
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/viewer_state_test.cpp"
  )
  target_link_libraries(
    granit_example_model_viewer_support_test
    PRIVATE granit_example_model_viewer_support Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_example_model_viewer_support_test)
  add_test(
    NAME granit.example.model_viewer_support COMMAND granit_example_model_viewer_support_test
  )
  if(WIN32)
    set_tests_properties(
      granit.example.model_viewer_support
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::render_pipeline>"
    )
  endif()
endif()

if(TARGET granit::integration_imgui)
  add_library(
    granit_example_model_viewer_imgui STATIC
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/viewer_panels.cpp"
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/viewer_panels.h"
  )
  target_compile_features(granit_example_model_viewer_imgui PUBLIC cxx_std_20)
  target_include_directories(
    granit_example_model_viewer_imgui PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples"
  )
  target_link_libraries(
    granit_example_model_viewer_imgui
    PUBLIC granit_example_model_viewer_support granit_example_imgui granit::integration_imgui
  )
  set_target_properties(granit_example_model_viewer_imgui PROPERTIES FOLDER "Examples")
  granit_target_compile_warnings(granit_example_model_viewer_imgui)

  if(GRANIT_HAS_NATIVE_WINDOW AND GRANIT_TESTING_ENABLED)
    add_executable(
    granit_example_model_viewer_imgui_test
    "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/viewer_panels_test.cpp"
    )
    target_link_libraries(
      granit_example_model_viewer_imgui_test
      PRIVATE granit_example_model_viewer_imgui Catch2::Catch2WithMain
    )
    granit_target_compile_warnings(granit_example_model_viewer_imgui_test)
    add_test(
      NAME granit.example.model_viewer_imgui COMMAND granit_example_model_viewer_imgui_test
    )
    if(WIN32)
      set_tests_properties(
        granit.example.model_viewer_imgui
        PROPERTIES ENVIRONMENT_MODIFICATION
                   "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::integration_imgui>"
      )
    endif()
  endif()
endif()
