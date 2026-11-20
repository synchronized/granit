# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 准备桌面构建树资产以及工具与测试共享的资产存储目标。
set(granit_build_tree_asset_dir "${CMAKE_CURRENT_BINARY_DIR}/granit-assets")
# 环境资产由外部工具预构建；配置阶段校验 Manifest 中的大小和 SHA-256，避免发布时携带
# 被意外替换或截断的二进制文件。
set(granit_environment_manifest
    "${granit_installed_asset_snapshot_dir}/environments/studio_small_03.manifest.json")
set(granit_environment_archive
    "${granit_installed_asset_snapshot_dir}/environments/studio_small_03.grenv")
if(NOT EXISTS "${granit_environment_manifest}" OR NOT EXISTS "${granit_environment_archive}")
  message(FATAL_ERROR "缺少标准环境资产或 Manifest：${granit_environment_archive}")
endif()
file(READ "${granit_environment_manifest}" granit_environment_manifest_json)
string(JSON granit_environment_expected_size GET "${granit_environment_manifest_json}" generated size)
string(JSON granit_environment_expected_sha256 GET "${granit_environment_manifest_json}" generated sha256)
file(SIZE "${granit_environment_archive}" granit_environment_actual_size)
file(SHA256 "${granit_environment_archive}" granit_environment_actual_sha256)
if(NOT granit_environment_actual_size STREQUAL granit_environment_expected_size OR
   NOT granit_environment_actual_sha256 STREQUAL granit_environment_expected_sha256)
  message(FATAL_ERROR "标准环境资产与 Manifest 不匹配：${granit_environment_archive}")
endif()
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
