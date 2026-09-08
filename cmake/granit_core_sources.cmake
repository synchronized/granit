# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard()

# 桌面 Vulkan 与 Emscripten WebGPU 两个后端共享的 granit 核心源文件。
#
# 两个构建入口（src/CMakeLists.txt 与 web/CMakeLists.txt）各自组装同一个 `granit` 目标，
# 后端无关的 Core、Assets 与 Renderer 公共源在此处统一维护，避免两份镜像清单漂移。
# 后端专用源（backend/vulkan、backend/webgpu）与各自的 renderer_factory 由调用方追加。

set(GRANIT_CORE_SOURCES
    # core
    "${PROJECT_SOURCE_DIR}/src/core/handle_table.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/async_operation_state.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/diagnostic_sink.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/lifecycle_validation.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/retirement_queue.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/resource_validation.cpp"
    # assets
    "${PROJECT_SOURCE_DIR}/src/assets/texture_asset.cpp"
    # renderer
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/async_operation_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/pipeline_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/buffer_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/command_recorder_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/frame_context_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_async.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_buffer.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_command.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_pipeline.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_presentation.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_readback.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_rendering.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_texture.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_timestamp.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_transfer.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/renderer_registry_warmup.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/sampler_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/shader_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/surface_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/swapchain_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/texture_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/texture_asset_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/timestamp_query_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/upload_batch_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/readback_batch_api.cpp"
    "${PROJECT_SOURCE_DIR}/src/renderer/pipeline_warmup_api.cpp"
)

# 桌面 Vulkan 后端专用源；由 src/CMakeLists.txt 在配置后端后追加。
set(GRANIT_VULKAN_BACKEND_SOURCES
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/device.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/frame_context.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/command_recorder.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/instance.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/loader.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/memory_allocator.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/physical_device.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/readback_context.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/resources.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/result.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/surface.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/swapchain.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/timestamp_query.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/upload_context.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/version_check.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/vma_implementation.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/vulkan_renderer_state.cpp"
    # 桌面默认后端工厂：create_default_renderer 选择 Vulkan 并拒绝 WebGPU。
    "${PROJECT_SOURCE_DIR}/src/backend/vulkan/renderer_factory.cpp"
)

# Emscripten WebGPU 后端专用源；由 web/CMakeLists.txt 追加。
set(GRANIT_WEBGPU_BACKEND_SOURCES
    "${PROJECT_SOURCE_DIR}/src/backend/webgpu/provider_dispatch.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/webgpu/provider.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/webgpu/command_adapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/webgpu/presentation_adapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/webgpu/pipeline_adapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/webgpu/resource_adapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/webgpu/renderer_state.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/webgpu/shader_adapter.cpp"
    "${PROJECT_SOURCE_DIR}/src/backend/webgpu/timestamp_adapter.cpp"
    # Web 默认后端工厂：create_default_renderer 选择 WebGPU 并拒绝 Vulkan。
    "${PROJECT_SOURCE_DIR}/src/backend/webgpu/renderer_factory.cpp"
)
