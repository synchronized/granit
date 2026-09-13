# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED MODULE OR NOT EXISTS "${MODULE}")
  message(FATAL_ERROR "必须提供可用的 GranitShaderToolchain.cmake")
endif()

set(GRANIT_SHADER_TOOLCHAIN_MODE off CACHE STRING "" FORCE)
set(GRANIT_DXC_EXECUTABLE "should-be-cleared" CACHE FILEPATH "" FORCE)
set(GRANIT_TINT_EXECUTABLE "should-be-cleared" CACHE FILEPATH "" FORCE)
include("${MODULE}")
granit_find_shader_toolchain()
if(GRANIT_DXC_EXECUTABLE OR GRANIT_TINT_EXECUTABLE)
  message(FATAL_ERROR "off 模式未清除 Shader 工具路径")
endif()
if(NOT GRANIT_SHADER_TOOLCHAIN_RELEASE_TAG OR NOT GRANIT_SHADER_TOOLCHAIN_ARCHIVE_SHA256 OR
   NOT GRANIT_SHADER_TOOLCHAIN_PACKAGE_NAME)
  message(FATAL_ERROR "Shader Toolchain 锁定元数据不完整")
endif()
