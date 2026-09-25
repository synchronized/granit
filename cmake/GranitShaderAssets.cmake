# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

# 从 HLSL-first 清单生成跨后端 Shader Library。源码树优先使用 granit_asset_tool target；
# 安装 Consumer 使用 granit_ASSET_TOOL_EXECUTABLE。调用前应通过
# GranitShaderToolchain.cmake 的 granit_find_shader_toolchain() 准备工具链。
function(granit_add_hlsl_shader_library)
  set(options ALL)
  set(one_value_args NAME MANIFEST OUTPUT CACHE_DIR TARGET REFERENCE)
  set(multi_value_args SOURCES)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_MANIFEST OR NOT ARG_OUTPUT OR NOT ARG_CACHE_DIR OR NOT ARG_TARGET)
    message(FATAL_ERROR "granit_add_hlsl_shader_library 缺少必要参数")
  endif()
  if(NOT GRANIT_SHADER_TOOLCHAIN_ROOT OR NOT GRANIT_DXC_EXECUTABLE OR
     NOT GRANIT_TINT_EXECUTABLE)
    message(FATAL_ERROR "从 HLSL 源清单构建 Shader Library 需要完整 Shader Toolchain 根目录")
  endif()

  if(TARGET granit_asset_tool OR NOT DEFINED granit_ASSET_TOOL_EXECUTABLE)
    set(asset_tool "$<TARGET_FILE:granit_asset_tool>")
    set(asset_tool_dependency granit_asset_tool)
  elseif(DEFINED granit_ASSET_TOOL_EXECUTABLE AND granit_ASSET_TOOL_EXECUTABLE AND
         EXISTS "${granit_ASSET_TOOL_EXECUTABLE}")
    set(asset_tool "${granit_ASSET_TOOL_EXECUTABLE}")
    set(asset_tool_dependency "${granit_ASSET_TOOL_EXECUTABLE}")
  else()
    message(FATAL_ERROR "当前构建未提供可用的 granit_asset_tool")
  endif()

  get_filename_component(output_directory "${ARG_OUTPUT}" DIRECTORY)
  set(commands
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARG_CACHE_DIR}" "${output_directory}"
      COMMAND
        "${asset_tool}" shader build-library --manifest "${ARG_MANIFEST}"
        --toolchain "${GRANIT_SHADER_TOOLCHAIN_ROOT}"
        --cache "${ARG_CACHE_DIR}" --output "${ARG_OUTPUT}")
  set(dependencies "${asset_tool_dependency}" "${ARG_MANIFEST}" ${ARG_SOURCES})
  if(ARG_REFERENCE)
    list(APPEND commands COMMAND "${CMAKE_COMMAND}" -E compare_files "${ARG_OUTPUT}"
                                 "${ARG_REFERENCE}")
    list(APPEND dependencies "${ARG_REFERENCE}")
  endif()
  set(stamp "${ARG_OUTPUT}.verified")
  list(APPEND commands COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}")
  add_custom_command(
    OUTPUT "${stamp}"
    BYPRODUCTS "${ARG_OUTPUT}"
    ${commands}
    DEPENDS ${dependencies}
    COMMENT "从 HLSL 源清单构建 Shader Library ${ARG_NAME}"
    VERBATIM
  )
  if(ARG_ALL)
    add_custom_target(${ARG_TARGET} ALL DEPENDS "${stamp}")
  else()
    add_custom_target(${ARG_TARGET} DEPENDS "${stamp}")
  endif()
endfunction()
