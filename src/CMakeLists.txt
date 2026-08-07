# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(granit)
add_library(granit::granit ALIAS granit)

set(GRANIT_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
set(GRANIT_GENERATED_VERSION_HEADER
    "${GRANIT_GENERATED_INCLUDE_DIR}/granit/version.h")

file(MAKE_DIRECTORY "${GRANIT_GENERATED_INCLUDE_DIR}/granit")
configure_file(
  "${PROJECT_SOURCE_DIR}/include/granit/version.h.in"
  "${GRANIT_GENERATED_VERSION_HEADER}"
  @ONLY
)

target_sources(
  granit
  PRIVATE
    $<TARGET_OBJECTS:granit_volk>
    backend/vulkan/device.cpp
    backend/vulkan/device.h
    backend/vulkan/instance.cpp
    backend/vulkan/instance.h
    backend/vulkan/loader.cpp
    backend/vulkan/loader.h
    backend/vulkan/physical_device.cpp
    backend/vulkan/physical_device.h
    backend/vulkan/result.cpp
    backend/vulkan/result.h
    backend/vulkan/version_check.cpp
    core/handle_table.cpp
    core/handle_table.h
    result.cpp
    version.cpp
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      "${PROJECT_SOURCE_DIR}/include"
      "${GRANIT_GENERATED_INCLUDE_DIR}"
    FILES
      "${PROJECT_SOURCE_DIR}/include/granit/export.h"
      "${PROJECT_SOURCE_DIR}/include/granit/granit.h"
      "${PROJECT_SOURCE_DIR}/include/granit/granit.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/result.h"
      "${PROJECT_SOURCE_DIR}/include/granit/result.hpp"
      "${PROJECT_SOURCE_DIR}/include/granit/types.h"
      "${PROJECT_SOURCE_DIR}/include/granit/types.hpp"
      "${GRANIT_GENERATED_VERSION_HEADER}"
)

target_compile_features(granit PUBLIC cxx_std_20)
granit_target_compile_warnings(granit)
target_include_directories(
  granit
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${GRANIT_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(
  granit
  SYSTEM PRIVATE
    "${GRANIT_VOLK_DIR}"
    "${GRANIT_VULKAN_HEADERS_DIR}"
)

target_compile_definitions(
  granit
  PRIVATE
    GRANIT_BUILDING_LIBRARY
    VK_NO_PROTOTYPES
    VOLK_NAMESPACE
  PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_STATIC_DEFINE>
)

target_link_libraries(granit PRIVATE ${CMAKE_DL_LIBS})

target_compile_options(
  granit
  PUBLIC
    $<$<COMPILE_LANG_AND_ID:C,MSVC>:/utf-8>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)

set_target_properties(
  granit
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

install(
  TARGETS granit
  EXPORT granitTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
  EXPORT granitTargets
  NAMESPACE granit::
  DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
)
