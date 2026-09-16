# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_BUILD_DIR OR
   NOT DEFINED GRANIT_INSTALL_PREFIX)
  message(FATAL_ERROR "必须提供 GRANIT_SOURCE_DIR、GRANIT_BUILD_DIR 和 GRANIT_INSTALL_PREFIX")
endif()

cmake_path(NORMAL_PATH GRANIT_SOURCE_DIR OUTPUT_VARIABLE source_dir)
cmake_path(NORMAL_PATH GRANIT_BUILD_DIR OUTPUT_VARIABLE build_dir)
cmake_path(NORMAL_PATH GRANIT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)

set(required_files
    "include/granit/granit.h"
    "include/granit/granit.hpp"
    "include/granit/renderer/native_surface.h"
    "include/granit/renderer/native_surface.hpp"
    "lib/cmake/granit/granitConfig.cmake"
    "lib/cmake/granit/granitConfigVersion.cmake"
    "lib/cmake/granit/granitTargets.cmake"
    "lib/cmake/granit/granitRenderPipelineTargets.cmake"
)
if(EXISTS "${install_prefix}/lib/cmake/granit/granitWindowTargets.cmake")
  list(APPEND required_files
       "include/granit/window/input.h"
       "include/granit/window/input.hpp"
       "include/granit/window/presentation.h"
       "include/granit/window/presentation.hpp"
       "include/granit/window/native.h"
       "include/granit/window/native.hpp")
endif()
foreach(required_file IN LISTS required_files)
  if(NOT EXISTS "${install_prefix}/${required_file}")
    message(FATAL_ERROR "安装结果缺少必要文件: ${required_file}")
  endif()
endforeach()

foreach(obsolete_file IN ITEMS
    "include/granit/input.h"
    "include/granit/input.hpp"
    "include/granit/input"
    "lib/cmake/granit/granitInputTargets.cmake")
  if(EXISTS "${install_prefix}/${obsolete_file}")
    message(FATAL_ERROR "0.25.0 安装结果仍包含旧 Input 入口：${obsolete_file}")
  endif()
endforeach()

foreach(aggregate_header IN ITEMS
    "include/granit/granit.h"
    "include/granit/granit.hpp"
    "include/granit/window/window.h"
    "include/granit/window/window.hpp"
    "include/granit/renderer/renderer.h"
    "include/granit/renderer/renderer.hpp"
    "include/granit/renderer/surface.h")
  if(EXISTS "${install_prefix}/${aggregate_header}")
    file(READ "${install_prefix}/${aggregate_header}" aggregate_content)
    foreach(native_marker IN ITEMS
        "<granit/window/native.h>"
        "<granit/window/native.hpp>"
        "<granit/renderer/native_surface.h>"
        "<granit/renderer/native_surface.hpp>")
      string(FIND "${aggregate_content}" "${native_marker}" native_position)
      if(NOT native_position EQUAL -1)
        message(FATAL_ERROR "普通入口 ${aggregate_header} 包含原生头：${native_marker}")
      endif()
    endforeach()
  endif()
endforeach()

if(EXISTS "${install_prefix}/lib/cmake/granit/granitAssetToolsTargets.cmake")
  foreach(toolchain_module IN ITEMS GranitShaderToolchain.cmake GranitShaderToolchainLock.cmake
                                   granit_shader_toolchain.cmake download_shader_toolchain.cmake
                                   verify_shader_toolchain_manifest.cmake)
    if(NOT EXISTS "${install_prefix}/lib/cmake/granit/${toolchain_module}")
      message(FATAL_ERROR "AssetTools 安装结果缺少工具链模块：${toolchain_module}")
    endif()
  endforeach()
  foreach(
    asset_tools_header
    IN ITEMS
      shader_compiler.h
      shader_compiler.hpp
      shader_library_builder.h
      shader_library_builder.hpp
      shader_reflection.h
      shader_reflection.hpp
      asset_tools.h
      asset_tools.hpp
      export.h
      environment_builder.h
      environment_builder.hpp
      material_builder.h
      material_builder.hpp
      texture_builder.h
      texture_builder.hpp
  )
    if(NOT EXISTS "${install_prefix}/include/granit/asset_tools/${asset_tools_header}")
      message(FATAL_ERROR "AssetTools 安装结果缺少公共头：${asset_tools_header}")
    endif()
  endforeach()
  if(EXISTS "${install_prefix}/include/granit/tools")
    message(FATAL_ERROR "AssetTools 安装结果仍包含旧公共头目录：include/granit/tools")
  endif()
  foreach(
    obsolete_asset_tools_file
    IN ITEMS
      include/granit/tools/shader_tools.h
      include/granit/tools/shader_tools.hpp
      include/granit/tools/shader_tools_export.h
      lib/cmake/granit/granitShaderToolsTargets.cmake
      bin/granit_shader_tool
      bin/granit_shader_tool.exe
      bin/granit_material_tool
      bin/granit_material_tool.exe
  )
    if(EXISTS "${install_prefix}/${obsolete_asset_tools_file}")
      message(FATAL_ERROR "AssetTools 安装结果仍包含旧文件：${obsolete_asset_tools_file}")
    endif()
  endforeach()
endif()

file(GLOB_RECURSE installed_files RELATIVE "${install_prefix}" "${install_prefix}/*")
if(NOT installed_files)
  message(FATAL_ERROR "安装前缀为空: ${install_prefix}")
endif()
foreach(installed_file IN LISTS installed_files)
  string(TOLOWER "${installed_file}" installed_file_lower)
  if(installed_file_lower MATCHES "(^|/)(tests?|3rd|catch2|unity|volk|vulkan)(/|$)")
    message(FATAL_ERROR "安装结果包含内部或第三方路径: ${installed_file}")
  endif()
  if(installed_file_lower MATCHES "\\.(grshaderobj|spv|wgsl|grshidx\\.json)$")
    message(FATAL_ERROR "安装结果包含构建期 Shader 产物: ${installed_file}")
  endif()
endforeach()

file(GLOB package_files "${install_prefix}/lib/cmake/granit/*.cmake")
foreach(package_file IN LISTS package_files)
  file(READ "${package_file}" content)
  foreach(forbidden IN ITEMS "${source_dir}" "${build_dir}" "Catch2::" "Unity::" "Vulkan::"
                             "granit_volk" "volk::" "vulkan-headers" "/3rd/")
    string(FIND "${content}" "${forbidden}" position)
    if(NOT position EQUAL -1)
      message(FATAL_ERROR "${package_file} 泄漏了私有内容: ${forbidden}")
    endif()
  endforeach()
endforeach()

message(STATUS "安装导出审计通过：${install_prefix}")
