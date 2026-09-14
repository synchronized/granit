# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 准备桌面构建树资产以及工具与测试共享的资产存储目标。
set(granit_build_tree_asset_dir "${CMAKE_CURRENT_BINARY_DIR}/granit-assets")
# 该目录完全由配置过程拥有；重建可清除 0.20 遗留的 Shader 中间产物和源 JSON。
file(REMOVE_RECURSE "${granit_build_tree_asset_dir}")
file(MAKE_DIRECTORY "${granit_build_tree_asset_dir}")
file(
  COPY "${granit_installed_asset_snapshot_dir}/libraries/pbr_standard.grshlib"
  DESTINATION "${granit_build_tree_asset_dir}/libraries"
)
file(
  COPY "${granit_installed_asset_snapshot_dir}/materials/pbr_standard.grmat"
  DESTINATION "${granit_build_tree_asset_dir}/materials"
)
file(
  COPY
    "${granit_installed_asset_snapshot_dir}/environments/studio_small_03.grenv"
    "${granit_installed_asset_snapshot_dir}/environments/studio_small_03.manifest.json"
  DESTINATION "${granit_build_tree_asset_dir}/environments"
)
set(
  granit_RENDER_PIPELINE_ASSET_DIR
  "${granit_build_tree_asset_dir}"
  CACHE INTERNAL
  "Granit 构建树 RenderPipeline 资产根目录"
  FORCE
)
