# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Model Viewer 私有 Core、工具和验收目标；由示例自身的目标编排包含。

set(granit_model_viewer_material_package
    "${CMAKE_CURRENT_BINARY_DIR}/model_viewer_pbr.grmat")
set(granit_model_viewer_material_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/model_viewer_pbr.grmat.inc")
set(granit_model_viewer_shader_library_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/pbr_standard.grshlib.inc")
add_custom_command(
  OUTPUT "${granit_model_viewer_shader_library_include}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
  COMMAND
    "${CMAKE_COMMAND}"
    "-DINPUT=${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
    "-DOUTPUT=${granit_model_viewer_shader_library_include}"
    -P "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  DEPENDS
    "${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
    "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  COMMENT "内嵌模型查看器 PBR Shader Library"
  VERBATIM
)
if(CMAKE_CROSSCOMPILING)
  set(
    granit_model_viewer_material_package
    "${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grmat"
  )
else()
  add_custom_command(
    OUTPUT "${granit_model_viewer_material_package}"
    COMMAND
      granit_asset_tool material build
      "${granit_asset_sources_dir}/materials/pbr_standard.grmat.json"
      --output "${granit_model_viewer_material_package}" --shader-index
      "${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grshidx.json"
    COMMAND
      "${CMAKE_COMMAND}" -E compare_files
      "${granit_model_viewer_material_package}"
      "${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grmat"
    DEPENDS
      granit_asset_tool
      "${granit_asset_sources_dir}/materials/pbr_standard.grmat.json"
      "${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grshidx.json"
      "${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grmat"
    COMMENT "生成模型查看器 PBR 材质归档"
    VERBATIM
  )
endif()
add_custom_command(
  OUTPUT "${granit_model_viewer_material_include}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
  COMMAND
    "${CMAKE_COMMAND}" "-DINPUT=${granit_model_viewer_material_package}"
    "-DOUTPUT=${granit_model_viewer_material_include}"
    -P "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  DEPENDS
    "${granit_model_viewer_material_package}"
    "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  COMMENT "内嵌模型查看器 PBR 材质归档"
  VERBATIM
)

add_library(
  granit_model_viewer_core STATIC
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/application_core.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/application_core.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/environment_ktx2.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/environment_ktx2.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/frame_executor.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/frame_executor.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/gpu_scene.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/gpu_scene.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/material_archive.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/material_archive.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/material_edit.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/orbit_camera.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/orbit_camera.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/performance_history.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/performance_history.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/viewer_state.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/viewer_state.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/viewer_input.h"
  "${granit_model_viewer_material_include}"
  "${granit_model_viewer_shader_library_include}"
)
add_library(granit_example_model_viewer_support ALIAS granit_model_viewer_core)
target_compile_features(granit_model_viewer_core PUBLIC cxx_std_20)
target_include_directories(
  granit_model_viewer_core
  PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples" "${PROJECT_SOURCE_DIR}/examples/common"
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
  granit_model_viewer_core
  PUBLIC granit_example_gltf_support granit::math granit::render_pipeline
         granit_example_imgui_canvas
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

if(NOT EMSCRIPTEN AND GRANIT_TESTING_ENABLED)
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

  if(NOT EMSCRIPTEN AND GRANIT_TESTING_ENABLED)
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
