# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 教程与完整 Model Viewer 共用的私有 glTF → GPU Scene 映射。
set(granit_model_scene_material_package
    "${CMAKE_CURRENT_BINARY_DIR}/model_viewer_pbr.grmat")
set(granit_model_scene_material_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/model_viewer_pbr.grmat.inc")
set(granit_model_scene_shader_library_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/pbr_standard.grshlib.inc")

add_custom_command(
  OUTPUT "${granit_model_scene_shader_library_include}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
  COMMAND
    "${CMAKE_COMMAND}"
    "-DINPUT=${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
    "-DOUTPUT=${granit_model_scene_shader_library_include}"
    -P "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  DEPENDS
    "${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
    "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  COMMENT "内嵌示例 PBR Shader Library"
  VERBATIM
)

if(CMAKE_CROSSCOMPILING)
  set(
    granit_model_scene_material_package
    "${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grmat"
  )
else()
  add_custom_command(
    OUTPUT "${granit_model_scene_material_package}"
    COMMAND
      granit_asset_tool material build
      "${granit_asset_sources_dir}/materials/pbr_standard.grmat.json"
      --output "${granit_model_scene_material_package}" --shader-library
      "${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
    COMMAND
      "${CMAKE_COMMAND}" -E compare_files
      "${granit_model_scene_material_package}"
      "${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grmat"
    DEPENDS
      granit_asset_tool
      "${granit_asset_sources_dir}/materials/pbr_standard.grmat.json"
      "${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
      "${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grmat"
    COMMENT "生成示例 PBR 材质归档"
    VERBATIM
  )
endif()

add_custom_command(
  OUTPUT "${granit_model_scene_material_include}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
  COMMAND
    "${CMAKE_COMMAND}" "-DINPUT=${granit_model_scene_material_package}"
    "-DOUTPUT=${granit_model_scene_material_include}"
    -P "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  DEPENDS
    "${granit_model_scene_material_package}"
    "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  COMMENT "内嵌示例 PBR 材质归档"
  VERBATIM
)

add_library(
  granit_example_model_scene STATIC
  gpu_scene.cpp
  gpu_scene.h
  material_archive.cpp
  material_archive.h
  material_edit.h
  "${granit_model_scene_material_include}"
  "${granit_model_scene_shader_library_include}"
)
target_compile_features(granit_example_model_scene PUBLIC cxx_std_20)
target_include_directories(
  granit_example_model_scene
  PUBLIC "${PROJECT_SOURCE_DIR}/examples/common"
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
  granit_example_model_scene
  PUBLIC granit_example_gltf_support granit::math granit::render_pipeline
)
set_target_properties(granit_example_model_scene PROPERTIES FOLDER "Examples/Common")
granit_target_compile_warnings(granit_example_model_scene)
