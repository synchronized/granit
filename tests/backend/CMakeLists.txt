# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set_property(DIRECTORY APPEND PROPERTY LABELS unit)

add_test(
  NAME granit.backend.native_boundaries
  COMMAND
    "${CMAKE_COMMAND}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -P "${PROJECT_SOURCE_DIR}/tests/cmake/check_backend_native_boundaries.cmake"
)


add_executable(granit_backend_capabilities_test capabilities_test.cpp)
target_include_directories(granit_backend_capabilities_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_backend_capabilities_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_backend_capabilities_test)
add_test(NAME granit.backend.capabilities COMMAND granit_backend_capabilities_test)

add_executable(granit_backend_resources_test resources_test.cpp)
target_include_directories(granit_backend_resources_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  granit_backend_resources_test
  PRIVATE granit::granit Catch2::Catch2WithMain
)
granit_target_compile_warnings(granit_backend_resources_test)
add_test(NAME granit.backend.resources COMMAND granit_backend_resources_test)


add_executable(
  granit_vulkan_backend_test
  vulkan_backend_test.cpp
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/device.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/frame_context.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/instance.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/loader.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/memory_allocator.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/physical_device.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/result.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/surface.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/swapchain.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/timestamp_query.cpp"
  "${PROJECT_SOURCE_DIR}/src/backend/vulkan/vma_implementation.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/diagnostic_sink.cpp"
  $<TARGET_OBJECTS:granit_volk>
)
target_include_directories(
  granit_vulkan_backend_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_include_directories(
  granit_vulkan_backend_test
  SYSTEM PRIVATE
    "${GRANIT_VOLK_DIR}"
    "${GRANIT_VMA_DIR}"
    "${GRANIT_VULKAN_HEADERS_DIR}"
)
target_compile_definitions(
  granit_vulkan_backend_test
  PRIVATE
    VK_NO_PROTOTYPES
    VOLK_NAMESPACE
    VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_VULKAN_VERSION=1003000
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
)
target_link_libraries(
  granit_vulkan_backend_test
  PRIVATE granit::granit Catch2::Catch2WithMain ${CMAKE_DL_LIBS}
)
granit_target_compile_warnings(granit_vulkan_backend_test)
add_test(NAME granit.backend.vulkan COMMAND granit_vulkan_backend_test)

foreach(
  target
  IN ITEMS granit_backend_capabilities_test granit_backend_resources_test
           granit_vulkan_backend_test
)
  granit_target_output_directories(${target})
endforeach()

set_tests_properties(
  granit.backend.vulkan
  PROPERTIES ENVIRONMENT_MODIFICATION
             "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit>"
)
