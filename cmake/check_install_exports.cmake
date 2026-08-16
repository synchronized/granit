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
    "lib/cmake/granit/granitConfig.cmake"
    "lib/cmake/granit/granitConfigVersion.cmake"
    "lib/cmake/granit/granitTargets.cmake"
    "lib/cmake/granit/granitRenderPipelineTargets.cmake"
)
foreach(required_file IN LISTS required_files)
  if(NOT EXISTS "${install_prefix}/${required_file}")
    message(FATAL_ERROR "安装结果缺少必要文件: ${required_file}")
  endif()
endforeach()

file(GLOB_RECURSE installed_files RELATIVE "${install_prefix}" "${install_prefix}/*")
if(NOT installed_files)
  message(FATAL_ERROR "安装前缀为空: ${install_prefix}")
endif()
foreach(installed_file IN LISTS installed_files)
  string(TOLOWER "${installed_file}" installed_file_lower)
  if(installed_file_lower MATCHES "(^|/)(tests?|3rd|catch2|unity|volk|vulkan)(/|$)")
    message(FATAL_ERROR "安装结果包含内部或第三方路径: ${installed_file}")
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
