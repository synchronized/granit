# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Runtime Bundle 只使用 Khronos 官方源代码构建，不从 Vulkan SDK 安装目录复制二进制。
# 提交哈希用于 CI 的可复现来源校验；升级时必须同步审阅许可证和已知依赖。
set(GRANIT_VULKAN_LOADER_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Loader.git")
set(GRANIT_VULKAN_LOADER_REF "a9e72c66d5cb79911eb9a9063bf4016dd0a3a123")
set(GRANIT_VULKAN_VALIDATION_REPOSITORY
    "https://github.com/KhronosGroup/Vulkan-ValidationLayers.git")
set(GRANIT_VULKAN_VALIDATION_REF "b9d4f9ead8d97c1bb0d174ea07d8aed8273818a5")
set(GRANIT_VULKAN_RUNTIME_VERSION "1.4.350")
