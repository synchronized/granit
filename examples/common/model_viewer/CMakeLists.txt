# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(granit_model_viewer_material_package
    "${CMAKE_CURRENT_BINARY_DIR}/model_viewer_pbr.grmat")
set(granit_model_viewer_material_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/model_viewer_pbr.grmat.inc")
if(CMAKE_CROSSCOMPILING)
  set(
    granit_model_viewer_material_package
    "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.grmat"
  )
else()
  add_custom_command(
    OUTPUT "${granit_model_viewer_material_package}"
    COMMAND
      granit_material_tool build
      "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.grmat.json"
      --output "${granit_model_viewer_material_package}"
    COMMAND
      "${CMAKE_COMMAND}" -E compare_files
      "${granit_model_viewer_material_package}"
      "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.grmat"
    DEPENDS
      granit_material_tool
      "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.grmat.json"
      "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.vert.spv"
      "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.vert.wgsl"
      "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.frag.spv"
      "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.frag.wgsl"
      "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.grmat"
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
    -P "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
  DEPENDS
    "${granit_model_viewer_material_package}"
    "${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake"
  COMMENT "内嵌模型查看器 PBR 材质归档"
  VERBATIM
)

add_library(
  granit_model_viewer_core STATIC
  application_core.cpp
  application_core.h
  environment_package.cpp
  environment_package.h
  environment_ktx2.cpp
  environment_ktx2.h
  environment_resources.cpp
  environment_resources.h
  gpu_scene.cpp
  material_archive.cpp
  material_archive.h
  material_edit.h
  orbit_camera.cpp
  orbit_camera.h
  performance_history.cpp
  performance_history.h
  screenshot_comparison.cpp
  screenshot_comparison.h
  viewer_state.cpp
  viewer_state.h
  viewer_input.h
  "${granit_model_viewer_material_include}"
)
add_library(granit_example_model_viewer_support ALIAS granit_model_viewer_core)
target_compile_features(granit_model_viewer_core PUBLIC cxx_std_20)
target_include_directories(
  granit_model_viewer_core
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/.."
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(
  granit_model_viewer_core
  PUBLIC granit_example_gltf_support granit::math granit::render_pipeline
)
set_target_properties(granit_model_viewer_core PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_model_viewer_core)

if(NOT CMAKE_CROSSCOMPILING)
  add_executable(granit_model_viewer_environment_tool environment_tool_main.cpp)
  target_link_libraries(
    granit_model_viewer_environment_tool
    PRIVATE granit_example_model_viewer_support granit_example_gltf_dependencies
  )
  set_target_properties(
    granit_model_viewer_environment_tool PROPERTIES FOLDER "Examples/Tools"
  )
  granit_target_output_directories(granit_model_viewer_environment_tool)
  granit_target_compile_warnings(granit_model_viewer_environment_tool)
  if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
    add_test(NAME granit.example.model_viewer_environment_tool.usage
             COMMAND granit_model_viewer_environment_tool)
    set_tests_properties(
      granit.example.model_viewer_environment_tool.usage PROPERTIES WILL_FAIL TRUE
    )
  endif()

  add_executable(granit_model_viewer_offscreen_acceptance offscreen_acceptance.cpp)
  target_link_libraries(
    granit_model_viewer_offscreen_acceptance PRIVATE granit_example_model_viewer_support
  )
  set_target_properties(
    granit_model_viewer_offscreen_acceptance PROPERTIES FOLDER "Examples/Acceptance"
  )
  granit_target_output_directories(granit_model_viewer_offscreen_acceptance)
  granit_target_compile_warnings(granit_model_viewer_offscreen_acceptance)
endif()

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_executable(
    granit_example_model_viewer_support_test
    application_core_test.cpp
    environment_package_test.cpp
    environment_ktx2_test.cpp
    gpu_scene_test.cpp
    orbit_camera_test.cpp
    performance_history_test.cpp
    screenshot_comparison_test.cpp
    viewer_state_test.cpp
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
    texture_registry.cpp
    texture_registry.h
    viewer_panels.cpp
    viewer_panels.h
  )
  target_compile_features(granit_example_model_viewer_imgui PUBLIC cxx_std_20)
  target_include_directories(
    granit_example_model_viewer_imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/.."
  )
  target_link_libraries(
    granit_example_model_viewer_imgui
    PUBLIC granit_example_model_viewer_support granit::integration_imgui
  )
  set_target_properties(granit_example_model_viewer_imgui PROPERTIES FOLDER "Examples")
  granit_target_compile_warnings(granit_example_model_viewer_imgui)

  if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
    add_executable(
      granit_example_model_viewer_imgui_test texture_registry_test.cpp viewer_panels_test.cpp
    )
    target_link_libraries(
      granit_example_model_viewer_imgui_test
      PRIVATE granit_example_model_viewer_imgui Catch2::Catch2WithMain
    )
    granit_target_compile_warnings(granit_example_model_viewer_imgui_test)
    add_test(
      NAME granit.example.model_viewer_imgui COMMAND granit_example_model_viewer_imgui_test
    )
  endif()
endif()
