# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

target_sources(
  granit PRIVATE
    provider_dispatch.cpp
    provider.cpp
    command_adapter.cpp
    presentation_adapter.cpp
    pipeline_adapter.cpp
    resource_adapter.cpp
    renderer_state.cpp
    renderer_state_commands.cpp
    renderer_state_pipeline.cpp
    renderer_state_presentation.cpp
    renderer_state_resources.cpp
    renderer_state_submission.cpp
    renderer_state_timestamp.cpp
    renderer_state_transfer.cpp
    shader_adapter.cpp
    timestamp_adapter.cpp
    renderer_factory.cpp
)

# 头文件只供后端实现编译；最终程序链接时仍需同一个 Emscripten Port。
include(granit_web)
granit_target_webgpu(granit)
