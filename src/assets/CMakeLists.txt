# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 运行时与离线工具共享的内部对象库，不提供公共别名或安装导出。
add_library(granit_internal_shader_asset_format OBJECT shader_asset.cpp shader_asset.h)
target_compile_features(granit_internal_shader_asset_format PRIVATE cxx_std_20)
target_include_directories(
  granit_internal_shader_asset_format PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
granit_target_compile_warnings(granit_internal_shader_asset_format)
set_target_properties(
  granit_internal_shader_asset_format PROPERTIES
    FOLDER "Core"
    POSITION_INDEPENDENT_CODE YES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
