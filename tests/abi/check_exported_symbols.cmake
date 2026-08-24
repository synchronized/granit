# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

foreach(required_var IN ITEMS GRANIT_ABI_COMPONENT GRANIT_ABI_LIBRARY GRANIT_ABI_SNAPSHOT
                              GRANIT_ABI_EXPORT_TOOL GRANIT_ABI_EXPORT_TOOL_KIND)
  if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
    message(FATAL_ERROR "缺少 ABI 导出检查参数：${required_var}")
  endif()
endforeach()

file(STRINGS "${GRANIT_ABI_SNAPSHOT}" expected_symbols REGEX "^granit_[A-Za-z0-9_]+$")
list(SORT expected_symbols)

if(GRANIT_ABI_EXPORT_TOOL_KIND STREQUAL "dumpbin")
  execute_process(
    COMMAND "${GRANIT_ABI_EXPORT_TOOL}" /exports "${GRANIT_ABI_LIBRARY}"
    RESULT_VARIABLE tool_result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_error
  )
elseif(GRANIT_ABI_EXPORT_TOOL_KIND STREQUAL "nm")
  execute_process(
    COMMAND "${GRANIT_ABI_EXPORT_TOOL}" -D --defined-only --format=posix "${GRANIT_ABI_LIBRARY}"
    RESULT_VARIABLE tool_result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_error
  )
else()
  message(FATAL_ERROR "未知 ABI 导出检查工具：${GRANIT_ABI_EXPORT_TOOL_KIND}")
endif()

if(NOT tool_result EQUAL 0)
  message(FATAL_ERROR "${GRANIT_ABI_COMPONENT} 导出读取失败：${tool_error}")
endif()

if(GRANIT_ABI_EXPORT_TOOL_KIND STREQUAL "nm")
  string(REPLACE "\r" "" tool_output "${tool_output}")
  string(REPLACE "\n" ";" tool_lines "${tool_output}")
  foreach(line IN LISTS tool_lines)
    if(line MATCHES "^(granit_[A-Za-z0-9_]+)[ \t]")
      list(APPEND actual_symbols "${CMAKE_MATCH_1}")
    endif()
  endforeach()
else()
  string(REGEX MATCHALL "granit_[A-Za-z0-9_]+" actual_symbols "${tool_output}")
  # dumpbin 的标题包含 DLL 文件名，不能把文件名误判为导出符号。
  list(REMOVE_ITEM actual_symbols granit_render_pipeline granit_window granit_input)
endif()
list(REMOVE_DUPLICATES actual_symbols)
list(SORT actual_symbols)

set(missing_symbols "${expected_symbols}")
foreach(symbol IN LISTS actual_symbols)
  list(REMOVE_ITEM missing_symbols "${symbol}")
endforeach()

set(unexpected_symbols "${actual_symbols}")
foreach(symbol IN LISTS expected_symbols)
  list(REMOVE_ITEM unexpected_symbols "${symbol}")
endforeach()
if(DEFINED GRANIT_ABI_PRIVATE_EXPORTS AND EXISTS "${GRANIT_ABI_PRIVATE_EXPORTS}")
  file(STRINGS "${GRANIT_ABI_PRIVATE_EXPORTS}" private_symbols REGEX "^granit_[A-Za-z0-9_]+$")
  foreach(symbol IN LISTS private_symbols)
    list(REMOVE_ITEM unexpected_symbols "${symbol}")
  endforeach()
endif()

if(missing_symbols OR unexpected_symbols)
  message(FATAL_ERROR
    "${GRANIT_ABI_COMPONENT} 公共 C ABI 导出与快照不一致。"
    "\n缺失：${missing_symbols}"
    "\n意外导出：${unexpected_symbols}"
  )
endif()
