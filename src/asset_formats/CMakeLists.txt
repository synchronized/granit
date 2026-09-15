# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

function(granit_configure_asset_format_target target)
  target_compile_features(${target} PRIVATE cxx_std_20)
  target_include_directories(
    ${target} PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include"
  )
  granit_target_compile_warnings(${target})
  set_target_properties(
    ${target}
    PROPERTIES
      FOLDER "Core/Asset Formats"
      POSITION_INDEPENDENT_CODE YES
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES
  )
endfunction()

add_library(
  granit_asset_format_shader OBJECT
  "${PROJECT_SOURCE_DIR}/src/core/sha256.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/sha256.h"
  shader/shader_cache_key.cpp
  shader/shader_cache_key.h
  shader/shader_library.cpp
  shader/shader_library.h
  shader/shader_object.cpp
  shader/shader_object.h
)
granit_configure_asset_format_target(granit_asset_format_shader)

add_library(
  granit_asset_format_material OBJECT
  material/material_archive.cpp
  material/material_archive.h
  material/material_metadata.cpp
  material/material_metadata.h
  material/material_package.cpp
  material/material_package.h
  material/material_package_archive.cpp
  material/material_package_archive.h
)
granit_configure_asset_format_target(granit_asset_format_material)

add_library(
  granit_asset_format_texture OBJECT
  texture/texture_asset.cpp
  texture/texture_asset.h
)
granit_configure_asset_format_target(granit_asset_format_texture)

add_library(
  granit_asset_format_environment OBJECT
  environment/environment_asset.cpp
  environment/environment_asset.h
)
granit_configure_asset_format_target(granit_asset_format_environment)
