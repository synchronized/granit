# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Shader 前端工具只在资产构建阶段使用，不应成为 granit 核心目标的链接或安装依赖。
set(GRANIT_SHADER_TOOLCHAIN_DAWN_VERSION "v20260720.160313")
set(GRANIT_SHADER_TOOLCHAIN_TINT_REVISION "0bc38adde72b79013536f8ce354b639ae19ae195")
set(GRANIT_SHADER_TOOLCHAIN_DXC_VERSION "1.8.0.4973")
set(GRANIT_SHADER_TOOLCHAIN_GLSLANG_VERSION "15.3.0")
set(
  GRANIT_SHADER_TOOLCHAIN_POLICY
  "compatible"
  CACHE STRING
  "Shader 工具链策略：compatible、locked 或 unchecked"
)
set_property(
  CACHE GRANIT_SHADER_TOOLCHAIN_POLICY
  PROPERTY STRINGS compatible locked unchecked
)
set(GRANIT_TINT_REVISION "" CACHE STRING "Tint/Dawn 的实际源码修订号")

set(
  GRANIT_SHADER_TOOLCHAIN_ROOT
  ""
  CACHE PATH
  "包含 bin/dxc、bin/glslangValidator 和 bin/tint 的 Granit Shader 工具链根目录"
)
set(GRANIT_DXC_EXECUTABLE "" CACHE FILEPATH "DXC 可执行文件")
set(GRANIT_TINT_EXECUTABLE "" CACHE FILEPATH "Tint 可执行文件")
set(GRANIT_GLSLANG_EXECUTABLE "" CACHE FILEPATH "glslangValidator 可执行文件")

function(granit_find_shader_toolchain)
  if(NOT GRANIT_SHADER_TOOLCHAIN_POLICY STREQUAL "compatible" AND
     NOT GRANIT_SHADER_TOOLCHAIN_POLICY STREQUAL "locked" AND
     NOT GRANIT_SHADER_TOOLCHAIN_POLICY STREQUAL "unchecked")
    message(FATAL_ERROR "GRANIT_SHADER_TOOLCHAIN_POLICY 必须是 compatible、locked 或 unchecked")
  endif()
  if(GRANIT_SHADER_TOOLCHAIN_ROOT)
    set(granit_shader_toolchain_bin "${GRANIT_SHADER_TOOLCHAIN_ROOT}/bin")
  endif()

  if(NOT GRANIT_DXC_EXECUTABLE)
    find_program(
      granit_dxc_executable
      NAMES dxc
      HINTS
        "${granit_shader_toolchain_bin}"
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
      NO_CACHE
    )
    set(GRANIT_DXC_EXECUTABLE "${granit_dxc_executable}" CACHE FILEPATH "DXC 可执行文件" FORCE)
  endif()
  if(NOT GRANIT_TINT_EXECUTABLE)
    find_program(
      granit_tint_executable
      NAMES tint
      HINTS "${granit_shader_toolchain_bin}"
      NO_CACHE
    )
    set(GRANIT_TINT_EXECUTABLE "${granit_tint_executable}" CACHE FILEPATH "Tint 可执行文件" FORCE)
  endif()
  if(NOT GRANIT_GLSLANG_EXECUTABLE)
    find_program(
      granit_glslang_executable
      NAMES glslangValidator
      HINTS
        "${granit_shader_toolchain_bin}"
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
      NO_CACHE
    )
    set(
      GRANIT_GLSLANG_EXECUTABLE
      "${granit_glslang_executable}"
      CACHE FILEPATH
      "glslangValidator 可执行文件"
      FORCE
    )
  endif()

  set(granit_dxc_usable OFF)
  if(GRANIT_DXC_EXECUTABLE)
    execute_process(
      COMMAND "${GRANIT_DXC_EXECUTABLE}" --version
      RESULT_VARIABLE granit_dxc_version_result
      OUTPUT_VARIABLE granit_dxc_version_output
      ERROR_VARIABLE granit_dxc_version_error
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_STRIP_TRAILING_WHITESPACE
    )
    string(
      FIND
      "${granit_dxc_version_output}"
      "${GRANIT_SHADER_TOOLCHAIN_DXC_VERSION}"
      granit_dxc_version_offset
    )
    if(GRANIT_SHADER_TOOLCHAIN_POLICY STREQUAL "unchecked")
      set(granit_dxc_usable ON)
      message(WARNING "Granit Shader Toolchain: DXC 未执行版本与能力约束检查")
    elseif(NOT granit_dxc_version_result EQUAL 0)
      message(STATUS "Granit Shader Toolchain: DXC 无法报告版本")
    elseif(NOT granit_dxc_version_offset EQUAL -1)
      set(granit_dxc_usable ON)
      message(STATUS "Granit Shader Toolchain: DXC ${GRANIT_SHADER_TOOLCHAIN_DXC_VERSION}")
    elseif(GRANIT_SHADER_TOOLCHAIN_POLICY STREQUAL "locked")
      message(FATAL_ERROR "DXC 不匹配锁定版本 ${GRANIT_SHADER_TOOLCHAIN_DXC_VERSION}")
    else()
      set(granit_dxc_usable ON)
      message(WARNING "DXC 版本未经 Granit 验证，将继续执行实际编译能力探测")
    endif()
  endif()

  set(granit_tint_usable OFF)
  if(GRANIT_TINT_EXECUTABLE)
    execute_process(
      COMMAND "${GRANIT_TINT_EXECUTABLE}" --help
      RESULT_VARIABLE granit_tint_probe_result
      OUTPUT_VARIABLE granit_tint_probe_output
      ERROR_VARIABLE granit_tint_probe_error
    )
    set(granit_tint_probe_text "${granit_tint_probe_output}${granit_tint_probe_error}")
    # 当前锁定 Tint 的帮助命令会返回 1；能力文本比该退出码更适合作为兼容性探针。
    if(GRANIT_SHADER_TOOLCHAIN_POLICY STREQUAL "unchecked")
      set(granit_tint_usable ON)
      message(WARNING "Granit Shader Toolchain: Tint 未执行能力约束检查")
    elseif(granit_tint_probe_text MATCHES "--input-format" AND
       granit_tint_probe_text MATCHES "wgsl" AND
       granit_tint_probe_text MATCHES "spirv" AND
       granit_tint_probe_text MATCHES "--format")
      set(granit_tint_usable ON)
      message(
        STATUS
        "Granit Shader Toolchain: Tint ${GRANIT_SHADER_TOOLCHAIN_DAWN_VERSION} 能力可用"
      )
    else()
      message(STATUS "Granit Shader Toolchain: Tint 缺少锁定工作流要求的转换能力")
    endif()
    if(GRANIT_SHADER_TOOLCHAIN_POLICY STREQUAL "locked" AND granit_tint_usable AND
       NOT GRANIT_TINT_REVISION STREQUAL GRANIT_SHADER_TOOLCHAIN_TINT_REVISION)
      message(
        FATAL_ERROR
        "locked 策略要求 GRANIT_TINT_REVISION=${GRANIT_SHADER_TOOLCHAIN_TINT_REVISION}"
      )
    endif()
  endif()

  set(granit_glslang_usable OFF)
  if(GRANIT_GLSLANG_EXECUTABLE)
    execute_process(
      COMMAND "${GRANIT_GLSLANG_EXECUTABLE}" --version
      RESULT_VARIABLE granit_glslang_version_result
      OUTPUT_VARIABLE granit_glslang_version_output
      ERROR_VARIABLE granit_glslang_version_error
    )
    string(
      FIND
      "${granit_glslang_version_output}${granit_glslang_version_error}"
      "${GRANIT_SHADER_TOOLCHAIN_GLSLANG_VERSION}"
      granit_glslang_version_offset
    )
    if(GRANIT_SHADER_TOOLCHAIN_POLICY STREQUAL "unchecked")
      set(granit_glslang_usable ON)
      message(WARNING "Granit Shader Toolchain: glslang 未执行版本与能力约束检查")
    elseif(NOT granit_glslang_version_result EQUAL 0)
      message(STATUS "Granit Shader Toolchain: glslang 无法报告版本")
    elseif(NOT granit_glslang_version_offset EQUAL -1)
      set(granit_glslang_usable ON)
      message(STATUS "Granit Shader Toolchain: glslang ${GRANIT_SHADER_TOOLCHAIN_GLSLANG_VERSION}")
    elseif(GRANIT_SHADER_TOOLCHAIN_POLICY STREQUAL "locked")
      message(FATAL_ERROR "glslang 不匹配锁定版本 ${GRANIT_SHADER_TOOLCHAIN_GLSLANG_VERSION}")
    else()
      set(granit_glslang_usable ON)
      message(WARNING "glslang 版本未经 Granit 验证，将继续执行实际编译能力探测")
    endif()
  endif()

  if(NOT granit_dxc_usable)
    set(GRANIT_DXC_EXECUTABLE "" CACHE FILEPATH "DXC 可执行文件" FORCE)
  endif()
  if(NOT granit_tint_usable)
    set(GRANIT_TINT_EXECUTABLE "" CACHE FILEPATH "Tint 可执行文件" FORCE)
  endif()
  if(NOT granit_glslang_usable)
    set(GRANIT_GLSLANG_EXECUTABLE "" CACHE FILEPATH "glslangValidator 可执行文件" FORCE)
  endif()

  set(GRANIT_DXC_EXECUTABLE "${GRANIT_DXC_EXECUTABLE}" PARENT_SCOPE)
  set(GRANIT_TINT_EXECUTABLE "${GRANIT_TINT_EXECUTABLE}" PARENT_SCOPE)
  set(GRANIT_GLSLANG_EXECUTABLE "${GRANIT_GLSLANG_EXECUTABLE}" PARENT_SCOPE)
endfunction()
