# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 准备桌面构建树资产以及工具与测试共享的资产存储目标。
set(granit_build_tree_asset_dir "${CMAKE_CURRENT_BINARY_DIR}/granit-assets")
file(MAKE_DIRECTORY "${granit_build_tree_asset_dir}")
file(
  COPY
    "${PROJECT_SOURCE_DIR}/assets/shaders"
    "${PROJECT_SOURCE_DIR}/assets/materials"
    "${PROJECT_SOURCE_DIR}/assets/environments"
  DESTINATION "${granit_build_tree_asset_dir}"
)
set(
  granit_RENDER_PIPELINE_ASSET_DIR
  "${granit_build_tree_asset_dir}"
  CACHE INTERNAL
  "Granit 构建树 RenderPipeline 资产根目录"
  FORCE
)

add_library(
  granit_shader_asset_storage OBJECT
  "${PROJECT_SOURCE_DIR}/tools/shader_asset.cpp"
  "${PROJECT_SOURCE_DIR}/tools/shader_asset.h"
)
target_compile_features(granit_shader_asset_storage PUBLIC cxx_std_20)
target_include_directories(
  granit_shader_asset_storage PUBLIC "${PROJECT_SOURCE_DIR}/tools" "${PROJECT_SOURCE_DIR}/src"
)
granit_target_compile_warnings(granit_shader_asset_storage)
set_target_properties(
  granit_shader_asset_storage PROPERTIES FOLDER "Tools" POSITION_INDEPENDENT_CODE YES
)
