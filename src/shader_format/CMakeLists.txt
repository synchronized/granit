# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 运行时与离线工具共享的私有 Shader 格式层，不提供公共别名或安装导出。
add_library(
  granit_internal_shader_format OBJECT
  ../core/sha256.cpp
  ../core/sha256.h
  shader_cache_key.cpp
  shader_cache_key.h
  shader_object.cpp
  shader_object.h
  shader_library.cpp
  shader_library.h
)
target_compile_features(granit_internal_shader_format PRIVATE cxx_std_20)
target_include_directories(
  granit_internal_shader_format
  PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include"
)
granit_target_compile_warnings(granit_internal_shader_format)
set_target_properties(
  granit_internal_shader_format PROPERTIES
    FOLDER "Core"
    POSITION_INDEPENDENT_CODE YES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
