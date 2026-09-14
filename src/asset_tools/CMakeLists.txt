# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(GRANIT_SPIRV_REFLECT_DIR "${PROJECT_SOURCE_DIR}/3rd/spirv-reflect-1.4.350.0")
set(GRANIT_SPIRV_HEADERS_DIR "${PROJECT_SOURCE_DIR}/3rd/spirv-headers-1.4.350.0/include")

add_library(granit_spirv_reflect OBJECT "${GRANIT_SPIRV_REFLECT_DIR}/spirv_reflect.c")
target_compile_definitions(granit_spirv_reflect PUBLIC SPIRV_REFLECT_USE_SYSTEM_SPIRV_H)
target_include_directories(
  granit_spirv_reflect
  BEFORE PUBLIC "${GRANIT_SPIRV_REFLECT_DIR}" "${GRANIT_SPIRV_HEADERS_DIR}"
)
set_target_properties(granit_spirv_reflect PROPERTIES FOLDER "ThirdParty/SPIRV-Reflect")

find_package(Threads REQUIRED)

add_library(
  granit_asset_tools_shader_library_source OBJECT
  shader/library_source_manifest.cpp
  shader/library_source_manifest.h
)
target_compile_features(granit_asset_tools_shader_library_source PUBLIC cxx_std_20)
target_include_directories(
  granit_asset_tools_shader_library_source
  PUBLIC "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include"
)
granit_target_compile_warnings(granit_asset_tools_shader_library_source)
set_target_properties(
  granit_asset_tools_shader_library_source
  PROPERTIES FOLDER "AssetTools" POSITION_INDEPENDENT_CODE YES
)

add_library(
  granit_asset_tools_shader_compiler OBJECT
  common/child_process.h
  common/process.cpp
  shader/compiler_internal.cpp
  shader/compiler_internal.h
)
target_compile_features(granit_asset_tools_shader_compiler PUBLIC cxx_std_20)
target_link_libraries(granit_asset_tools_shader_compiler PRIVATE Threads::Threads)
target_include_directories(
  granit_asset_tools_shader_compiler PUBLIC "${PROJECT_SOURCE_DIR}/src"
)
target_include_directories(
  granit_asset_tools_shader_compiler
  SYSTEM PRIVATE "${GRANIT_SPIRV_REFLECT_DIR}" "${GRANIT_SPIRV_HEADERS_DIR}"
)
target_compile_definitions(
  granit_asset_tools_shader_compiler PRIVATE SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
)
granit_target_compile_warnings(granit_asset_tools_shader_compiler)
set_target_properties(
  granit_asset_tools_shader_compiler
  PROPERTIES FOLDER "AssetTools" POSITION_INDEPENDENT_CODE YES
)

add_library(
  granit_asset_tools_material_core OBJECT
  material/debug_json.cpp
  material/source_json.cpp
)
target_compile_features(granit_asset_tools_material_core PUBLIC cxx_std_20)
target_include_directories(
  granit_asset_tools_material_core
  PUBLIC "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
granit_target_compile_warnings(granit_asset_tools_material_core)
set_target_properties(
  granit_asset_tools_material_core PROPERTIES FOLDER "AssetTools" POSITION_INDEPENDENT_CODE YES
)

add_library(
  granit_asset_tools_shader_object_storage OBJECT
  shader/object_storage.cpp
  shader/object_storage.h
)
target_compile_features(granit_asset_tools_shader_object_storage PUBLIC cxx_std_20)
target_include_directories(
  granit_asset_tools_shader_object_storage
  PUBLIC "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/include"
)
granit_target_compile_warnings(granit_asset_tools_shader_object_storage)
set_target_properties(
  granit_asset_tools_shader_object_storage
  PROPERTIES FOLDER "AssetTools" POSITION_INDEPENDENT_CODE YES
)

if(GRANIT_BUILD_ASSET_TOOLS OR GRANIT_BUILD_TOOLS OR (GRANIT_BUILD_TESTING AND BUILD_TESTING))
  add_library(
    granit_asset_tools
    environment/builder_api.cpp
    material/builder_api.cpp
    shader/compiler_api.cpp
    shader/library_builder.cpp
    shader/library_builder.h
    shader/library_source_builder.cpp
    shader/object_cache.cpp
    shader/object_cache.h
    texture/builder_api.cpp
  )
  add_library(granit::asset_tools ALIAS granit_asset_tools)
  target_sources(
    granit_asset_tools
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
      FILES
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/export.h"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/environment_builder.h"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/environment_builder.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/material_builder.h"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/material_builder.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/shader_compiler.h"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/shader_compiler.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/shader_library_builder.h"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/shader_library_builder.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/shader_reflection.h"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/shader_reflection.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/texture_builder.h"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/texture_builder.hpp"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/asset_tools.h"
        "${PROJECT_SOURCE_DIR}/include/granit/asset_tools/asset_tools.hpp"
  )
  target_compile_features(granit_asset_tools PUBLIC cxx_std_20)
  target_include_directories(
    granit_asset_tools
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
    PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  )
  target_sources(
    granit_asset_tools
    PRIVATE
      $<TARGET_OBJECTS:granit_asset_tools_shader_object_storage>
      $<TARGET_OBJECTS:granit_asset_tools_shader_library_source>
      $<TARGET_OBJECTS:granit_asset_format_shader>
      $<TARGET_OBJECTS:granit_asset_format_material>
      $<TARGET_OBJECTS:granit_asset_format_texture>
      $<TARGET_OBJECTS:granit_asset_format_environment>
      $<TARGET_OBJECTS:granit_asset_tools_material_core>
      $<TARGET_OBJECTS:granit_asset_tools_shader_compiler>
      $<TARGET_OBJECTS:granit_spirv_reflect>
  )
  target_link_libraries(granit_asset_tools PRIVATE Threads::Threads)
  target_compile_definitions(
    granit_asset_tools
    PRIVATE GRANIT_ASSET_TOOLS_BUILDING_LIBRARY
    PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:GRANIT_ASSET_TOOLS_STATIC_DEFINE>
  )
  granit_target_compile_warnings(granit_asset_tools)
  granit_target_output_directories(granit_asset_tools)
  set_target_properties(
    granit_asset_tools
    PROPERTIES
      EXPORT_NAME asset_tools
      FOLDER "AssetTools"
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES
  )
  install(
    TARGETS granit_asset_tools
    EXPORT granitAssetToolsTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT AssetTools
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT AssetTools
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT AssetTools
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT AssetTools
  )
  install(
    EXPORT granitAssetToolsTargets
    NAMESPACE granit::
    DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
    COMPONENT AssetTools
  )
endif()
