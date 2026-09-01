# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(granit_model_viewer_material_package
    "${CMAKE_CURRENT_BINARY_DIR}/model_viewer_pbr.grmat")
set(granit_model_viewer_material_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/model_viewer_pbr.grmat.inc")
add_custom_command(
  OUTPUT "${granit_model_viewer_material_package}"
  COMMAND
    granit_material_tool build
    "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.grmat.json"
    --output "${granit_model_viewer_material_package}"
  DEPENDS
    granit_material_tool
    "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.grmat.json"
    "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.vert.spv"
    "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.vert.wgsl"
    "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.frag.spv"
    "${PROJECT_SOURCE_DIR}/examples/assets/model_viewer_pbr.frag.wgsl"
  COMMENT "生成模型查看器 PBR 材质归档"
  VERBATIM
)
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
  granit_example_model_viewer_support STATIC
  gpu_scene.cpp
  material_archive.cpp
  material_archive.h
  "${granit_model_viewer_material_include}"
)
target_compile_features(granit_example_model_viewer_support PUBLIC cxx_std_20)
target_include_directories(
  granit_example_model_viewer_support
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/.."
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(
  granit_example_model_viewer_support PUBLIC granit_example_gltf_support granit::render_pipeline
)
set_target_properties(granit_example_model_viewer_support PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_example_model_viewer_support)

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_executable(granit_example_model_viewer_support_test gpu_scene_test.cpp)
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
