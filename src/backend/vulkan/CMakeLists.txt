# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

target_sources(
  granit PRIVATE
    device.cpp
    frame_context.cpp
    command_recorder.cpp
    instance.cpp
    loader.cpp
    memory_allocator.cpp
    physical_device.cpp
    readback_context.cpp
    resources.cpp
    result.cpp
    surface.cpp
    swapchain.cpp
    timestamp_query.cpp
    upload_context.cpp
    version_check.cpp
    vma_implementation.cpp
    vulkan_renderer_state.cpp
    renderer_factory.cpp
    $<TARGET_OBJECTS:granit_volk>
    device.h
    frame_context.h
    command_recorder.h
    instance.h
    loader.h
    memory_allocator.h
    physical_device.h
    result.h
    resources.h
    readback_context.h
    surface.h
    swapchain.h
    timestamp_query.h
    upload_context.h
    vulkan_renderer_state.h
)

target_include_directories(
  granit
  SYSTEM PRIVATE
    "${GRANIT_VOLK_DIR}"
    "${GRANIT_VMA_DIR}"
    "${GRANIT_VULKAN_HEADERS_DIR}"
)

target_compile_definitions(
  granit
  PRIVATE
    VK_NO_PROTOTYPES
    VOLK_NAMESPACE
    VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_VULKAN_VERSION=1003000
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
    $<$<BOOL:${GRANIT_HAS_XCB}>:GRANIT_HAS_XCB>
    $<$<BOOL:${GRANIT_HAS_XCB}>:VK_USE_PLATFORM_XCB_KHR>
    $<$<BOOL:${GRANIT_HAS_WAYLAND}>:GRANIT_HAS_WAYLAND>
    $<$<BOOL:${GRANIT_HAS_WAYLAND}>:VK_USE_PLATFORM_WAYLAND_KHR>
)

target_link_libraries(granit PRIVATE ${CMAKE_DL_LIBS})
target_include_directories(
  granit SYSTEM PRIVATE
    $<$<BOOL:${GRANIT_HAS_XCB}>:${GRANIT_XCB_INCLUDE_DIR}>
    $<$<BOOL:${GRANIT_HAS_WAYLAND}>:${GRANIT_WAYLAND_INCLUDE_DIR}>
)
