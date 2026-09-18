# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# Shader 前端工具只在资产构建阶段使用，不应成为 granit 核心目标的链接或安装依赖。
include("${CMAKE_CURRENT_LIST_DIR}/GranitShaderToolchainLock.cmake")
set(
  GRANIT_SHADER_TOOLCHAIN_MODE
  "system"
  CACHE STRING
  "Shader 工具链获取模式：off、system、auto 或 download"
)
set_property(CACHE GRANIT_SHADER_TOOLCHAIN_MODE PROPERTY STRINGS off system auto download)
set(
  GRANIT_SHADER_TOOLCHAIN_CACHE_DIR
  "${CMAKE_BINARY_DIR}/toolchains"
  CACHE PATH
  "锁定 Shader 工具链下载缓存目录"
)
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
  "包含 bin/dxc 和 bin/tint 的 Granit Shader 工具链根目录"
)
set(GRANIT_DXC_EXECUTABLE "" CACHE FILEPATH "DXC 可执行文件")
set(GRANIT_TINT_EXECUTABLE "" CACHE FILEPATH "Tint 可执行文件")

function(granit_find_shader_toolchain)
  granit_validate_enum(GRANIT_SHADER_TOOLCHAIN_MODE   off system auto download)
  granit_validate_enum(GRANIT_SHADER_TOOLCHAIN_POLICY compatible locked unchecked)

  if(GRANIT_SHADER_TOOLCHAIN_MODE STREQUAL "off")
    set(GRANIT_DXC_EXECUTABLE "" CACHE FILEPATH "DXC 可执行文件" FORCE)
    set(GRANIT_TINT_EXECUTABLE "" CACHE FILEPATH "Tint 可执行文件" FORCE)
    set(GRANIT_DXC_EXECUTABLE "" PARENT_SCOPE)
    set(GRANIT_TINT_EXECUTABLE "" PARENT_SCOPE)
    message(STATUS "Granit Shader Toolchain: 已禁用")
    return()
  endif()
  if(GRANIT_SHADER_TOOLCHAIN_MODE STREQUAL "download" AND NOT GRANIT_SHADER_TOOLCHAIN_ROOT)
    set(result_file "${CMAKE_BINARY_DIR}/granit-shader-toolchain-root.txt")
    execute_process(
      COMMAND
        "${CMAKE_COMMAND}" "-DDESTINATION=${GRANIT_SHADER_TOOLCHAIN_CACHE_DIR}"
        "-DRESULT_FILE=${result_file}" -P
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/download_shader_toolchain.cmake"
      RESULT_VARIABLE download_result
    )
    if(NOT download_result EQUAL 0 OR NOT EXISTS "${result_file}")
      message(FATAL_ERROR "无法获取锁定的 Granit Shader Toolchain")
    endif()
    file(READ "${result_file}" downloaded_root)
    set(GRANIT_SHADER_TOOLCHAIN_ROOT "${downloaded_root}" CACHE PATH
        "包含 bin/dxc 和 bin/tint 的 Granit Shader 工具链根目录" FORCE)
    set(GRANIT_DXC_EXECUTABLE "" CACHE FILEPATH "DXC 可执行文件" FORCE)
    set(GRANIT_TINT_EXECUTABLE "" CACHE FILEPATH "Tint 可执行文件" FORCE)
    set(GRANIT_TINT_REVISION "${GRANIT_SHADER_TOOLCHAIN_TINT_REVISION}" CACHE STRING
        "Tint/Dawn 的实际源码修订号" FORCE)
  endif()
  if(GRANIT_SHADER_TOOLCHAIN_ROOT)
    set(granit_shader_toolchain_bin "${GRANIT_SHADER_TOOLCHAIN_ROOT}/bin")
  endif()

  if(NOT GRANIT_DXC_EXECUTABLE)
    find_program(
      GRANIT_DXC_EXECUTABLE
      NAMES dxc
      HINTS
        "${granit_shader_toolchain_bin}"
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
      DOC "DXC 可执行文件"
    )
  endif()
  if(NOT GRANIT_TINT_EXECUTABLE)
    find_program(
      GRANIT_TINT_EXECUTABLE
      NAMES tint
      HINTS "${granit_shader_toolchain_bin}"
      DOC "Tint 可执行文件"
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

  if(NOT granit_dxc_usable)
    set(GRANIT_DXC_EXECUTABLE "" CACHE FILEPATH "DXC 可执行文件" FORCE)
  endif()
  if(NOT granit_tint_usable)
    set(GRANIT_TINT_EXECUTABLE "" CACHE FILEPATH "Tint 可执行文件" FORCE)
  endif()
  if(NOT GRANIT_SHADER_TOOLCHAIN_ROOT AND GRANIT_DXC_EXECUTABLE AND GRANIT_TINT_EXECUTABLE)
    get_filename_component(granit_dxc_bin "${GRANIT_DXC_EXECUTABLE}" DIRECTORY REALPATH)
    get_filename_component(granit_tint_bin "${GRANIT_TINT_EXECUTABLE}" DIRECTORY REALPATH)
    get_filename_component(granit_bin_name "${granit_dxc_bin}" NAME)
    if(granit_dxc_bin STREQUAL granit_tint_bin AND granit_bin_name STREQUAL "bin")
      get_filename_component(granit_inferred_root "${granit_dxc_bin}" DIRECTORY)
      set(GRANIT_SHADER_TOOLCHAIN_ROOT "${granit_inferred_root}" CACHE PATH
          "包含 bin/dxc 和 bin/tint 的 Granit Shader 工具链根目录" FORCE)
      message(STATUS "Granit Shader Toolchain: 从共同 bin 目录推导根目录 ${granit_inferred_root}")
    else()
      message(STATUS "Granit Shader Toolchain: DXC 与 Tint 不属于同一工具链根目录")
      set(GRANIT_DXC_EXECUTABLE "" CACHE FILEPATH "DXC 可执行文件" FORCE)
      set(GRANIT_TINT_EXECUTABLE "" CACHE FILEPATH "Tint 可执行文件" FORCE)
    endif()
  endif()
  if(GRANIT_SHADER_TOOLCHAIN_MODE STREQUAL "auto" AND
     (NOT GRANIT_DXC_EXECUTABLE OR NOT GRANIT_TINT_EXECUTABLE) AND
     NOT GRANIT_SHADER_TOOLCHAIN_ROOT)
    set(GRANIT_SHADER_TOOLCHAIN_MODE "download")
    granit_find_shader_toolchain()
    set(GRANIT_SHADER_TOOLCHAIN_ROOT "${GRANIT_SHADER_TOOLCHAIN_ROOT}" PARENT_SCOPE)
    set(GRANIT_DXC_EXECUTABLE "${GRANIT_DXC_EXECUTABLE}" PARENT_SCOPE)
    set(GRANIT_TINT_EXECUTABLE "${GRANIT_TINT_EXECUTABLE}" PARENT_SCOPE)
    return()
  endif()
  set(GRANIT_DXC_EXECUTABLE "${GRANIT_DXC_EXECUTABLE}" PARENT_SCOPE)
  set(GRANIT_TINT_EXECUTABLE "${GRANIT_TINT_EXECUTABLE}" PARENT_SCOPE)
endfunction()
