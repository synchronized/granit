# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 教程与完整 Model Viewer 共用的私有 glTF → Granit 渲染资源映射。
set(granit_gltf_rendering_material_package
    "${CMAKE_CURRENT_BINARY_DIR}/standard_pbr.grmat")
set(granit_gltf_rendering_material_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/standard_pbr.grmat.inc")
set(granit_gltf_rendering_shader_library_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/pbr_standard.grshlib.inc")
set(granit_gltf_rendering_resource_table_shader_library_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated/pbr_resource_table.grshlib.inc")

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

if(GRANIT_PBR_RESOURCE_TABLE_SHADER_LIBRARY)
  add_custom_command(
    OUTPUT "${granit_gltf_rendering_resource_table_shader_library_include}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DINPUT=${GRANIT_PBR_RESOURCE_TABLE_SHADER_LIBRARY}"
      "-DOUTPUT=${granit_gltf_rendering_resource_table_shader_library_include}"
      -P "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
    DEPENDS granit_pbr_resource_table_shader_library
            "${PROJECT_SOURCE_DIR}/cmake/assets/embed_binary.cmake"
    COMMENT "内嵌 Vulkan Resource Table PBR Shader Library"
    VERBATIM
  )
endif()

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
      --shader-library-implementation 2
      --shader-library "${GRANIT_PBR_RESOURCE_TABLE_SHADER_LIBRARY}"
    DEPENDS
      granit_asset_tool
      granit_pbr_resource_table_shader_library
      "${granit_asset_sources_dir}/materials/pbr_standard.grmat.json"
      "${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
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
  scene_resources.cpp
  scene_resources.h
  scene_plan.cpp
  scene_plan.h
  scene_snapshot.cpp
  scene_upload.cpp
  scene_upload.h
  standard_pbr_assets.cpp
  standard_pbr_assets.h
  material_update.h
  "${granit_gltf_rendering_material_include}"
  "${granit_gltf_rendering_shader_library_include}"
)
if(GRANIT_PBR_RESOURCE_TABLE_SHADER_LIBRARY)
  target_sources(granit_example_gltf_rendering PRIVATE
                 "${granit_gltf_rendering_resource_table_shader_library_include}")
  target_compile_definitions(granit_example_gltf_rendering
                             PRIVATE GRANIT_EXAMPLE_HAS_RESOURCE_TABLE_SHADER_LIBRARY=1)
endif()
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
  add_executable(granit_example_gltf_rendering_test scene_resources_test.cpp)
  target_link_libraries(
    granit_example_gltf_rendering_test
    PRIVATE granit_example_gltf_rendering granit_test_catch2_with_main
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
