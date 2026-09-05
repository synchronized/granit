# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

# 添加运行时与离线工具共享的私有 Shader Asset 格式实现。
function(granit_add_shader_asset_format)
  if(TARGET granit_shader_asset_format)
    return()
  endif()
  add_library(
    granit_shader_asset_format OBJECT
    "${PROJECT_SOURCE_DIR}/src/assets/shader_asset.cpp"
    "${PROJECT_SOURCE_DIR}/src/assets/shader_asset.h"
  )
  target_compile_features(granit_shader_asset_format PUBLIC cxx_std_20)
  target_include_directories(granit_shader_asset_format PUBLIC "${PROJECT_SOURCE_DIR}/src")
  granit_target_compile_warnings(granit_shader_asset_format)
  set_target_properties(
    granit_shader_asset_format PROPERTIES FOLDER "Core" POSITION_INDEPENDENT_CODE YES
  )
endfunction()
