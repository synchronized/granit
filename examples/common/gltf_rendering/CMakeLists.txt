# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 教程与完整 Model Viewer 共用的私有 glTF → GPU Scene 映射。
set(granit_gltf_rendering_material_package
    "${CMAKE_CURRENT_BINARY_DIR}/standard_pbr.grmat")
set(granit_gltf_rendering_material_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/standard_pbr.grmat.inc")
set(granit_gltf_rendering_shader_library_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/pbr_standard.grshlib.inc")

add_custom_command(
  OUTPUT "${granit_gltf_rendering_shader_library_include}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
  COMMAND
    "${CMAKE_COMMAND}"
    "-DINPUT=${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
    "-DOUTPUT=${granit_gltf_rendering_shader_library_include}"
    -P "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  DEPENDS
    "${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
    "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  COMMENT "内嵌示例 PBR Shader Library"
  VERBATIM
)

if(CMAKE_CROSSCOMPILING)
  set(
    granit_gltf_rendering_material_package
    "${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grmat"
  )
else()
  add_custom_command(
    OUTPUT "${granit_gltf_rendering_material_package}"
    COMMAND
      granit_asset_tool material build
      "${granit_asset_sources_dir}/materials/pbr_standard.grmat.json"
      --output "${granit_gltf_rendering_material_package}" --shader-library
      "${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
    COMMAND
      "${CMAKE_COMMAND}" -E compare_files
      "${granit_gltf_rendering_material_package}"
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
  OUTPUT "${granit_gltf_rendering_material_include}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
  COMMAND
    "${CMAKE_COMMAND}" "-DINPUT=${granit_gltf_rendering_material_package}"
    "-DOUTPUT=${granit_gltf_rendering_material_include}"
    -P "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  DEPENDS
    "${granit_gltf_rendering_material_package}"
    "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
  COMMENT "内嵌示例 PBR 材质归档"
  VERBATIM
)

add_library(
  granit_example_gltf_rendering STATIC
  gpu_scene.cpp
  gpu_scene.h
  scene_plan.cpp
  scene_plan.h
  standard_pbr_assets.cpp
  standard_pbr_assets.h
  material_update.h
  "${granit_gltf_rendering_material_include}"
  "${granit_gltf_rendering_shader_library_include}"
)
target_compile_features(granit_example_gltf_rendering PUBLIC cxx_std_20)
target_include_directories(
  granit_example_gltf_rendering
  PUBLIC "${PROJECT_SOURCE_DIR}/examples/common"
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(
  granit_example_gltf_rendering
  PUBLIC granit_example_gltf_support granit::render_pipeline
)
set_target_properties(granit_example_gltf_rendering PROPERTIES FOLDER "Examples/Common")
granit_target_compile_warnings(granit_example_gltf_rendering)

if(GRANIT_HAS_NATIVE_WINDOW AND GRANIT_TESTING_ENABLED)
  add_executable(granit_example_gltf_rendering_test gpu_scene_test.cpp)
  target_link_libraries(
    granit_example_gltf_rendering_test
    PRIVATE granit_example_gltf_rendering Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_example_gltf_rendering_test)
  add_test(NAME granit.example.gltf_rendering COMMAND granit_example_gltf_rendering_test)
  if(WIN32)
    set_tests_properties(
      granit.example.gltf_rendering
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::render_pipeline>"
    )
  endif()
endif()
