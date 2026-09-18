# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(GRANIT_HAS_VULKAN_BACKEND)
  set(GRANIT_VULKAN_HEADERS_DIR
      "${CMAKE_CURRENT_SOURCE_DIR}/Vulkan-Headers-1.4.350/include")
  set(GRANIT_VOLK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/volk-1.4.350")
  set(GRANIT_VMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vma-v3.3.0/include")

  # Volk 作为内部对象并入 Granit，避免静态安装包导出私有第三方 target。
  add_library(granit_volk OBJECT "${GRANIT_VOLK_DIR}/volk.c")
  set_source_files_properties("${GRANIT_VOLK_DIR}/volk.c" PROPERTIES LANGUAGE CXX)
  target_compile_features(granit_volk PRIVATE cxx_std_20)
  target_compile_definitions(
    granit_volk
    PRIVATE
      VOLK_NAMESPACE
      $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
      $<$<BOOL:${GRANIT_HAS_XCB}>:VK_USE_PLATFORM_XCB_KHR>
      $<$<BOOL:${GRANIT_HAS_WAYLAND}>:VK_USE_PLATFORM_WAYLAND_KHR>
  )
  target_include_directories(
    granit_volk
    SYSTEM PRIVATE
      "${GRANIT_VOLK_DIR}"
      "${GRANIT_VULKAN_HEADERS_DIR}"
      $<$<BOOL:${GRANIT_HAS_XCB}>:${GRANIT_XCB_INCLUDE_DIR}>
      $<$<BOOL:${GRANIT_HAS_WAYLAND}>:${GRANIT_WAYLAND_INCLUDE_DIR}>
  )
  set_target_properties(
    granit_volk
    PROPERTIES
      CXX_VISIBILITY_PRESET hidden
      POSITION_INDEPENDENT_CODE ON
      VISIBILITY_INLINES_HIDDEN YES
      FOLDER "ThirdParty/Volk"
  )

  set(GRANIT_VULKAN_HEADERS_DIR "${GRANIT_VULKAN_HEADERS_DIR}" PARENT_SCOPE)
  set(GRANIT_VOLK_DIR "${GRANIT_VOLK_DIR}" PARENT_SCOPE)
  set(GRANIT_VMA_DIR "${GRANIT_VMA_DIR}" PARENT_SCOPE)
endif()

if(GRANIT_TESTING_ENABLED)
  include("${CMAKE_CURRENT_LIST_DIR}/unity.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/catch2.cmake")
endif()
