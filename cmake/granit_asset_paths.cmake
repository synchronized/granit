# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 仓库资产按作者输入和已提交生成快照分离；构建输出始终写入 Build Tree。
set(granit_asset_sources_dir "${PROJECT_SOURCE_DIR}/assets/sources")
set(granit_installed_asset_snapshot_dir
    "${PROJECT_SOURCE_DIR}/assets/generated/installed")
set(granit_embedded_asset_snapshot_dir
    "${PROJECT_SOURCE_DIR}/assets/generated/embedded")
