# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

foreach(required COLLECTOR WORK_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "缺少 ${required}")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/source/a" "${WORK_DIR}/source/b")
file(WRITE "${WORK_DIR}/source/LICENSE" "root license\n")
file(WRITE "${WORK_DIR}/source/a/NOTICE.txt" "notice a\n")
file(WRITE "${WORK_DIR}/source/b/COPYING" "copying b\n")
file(WRITE "${WORK_DIR}/source/b/README" "must not be collected\n")
set(output "${WORK_DIR}/THIRD_PARTY_LICENSES.txt")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -DROOT=${WORK_DIR}/source -DOUTPUT=${output}
          -DCOMPONENT=fixture -P "${COLLECTOR}"
  RESULT_VARIABLE collect_result
  OUTPUT_VARIABLE collect_output
  ERROR_VARIABLE collect_error
)
if(NOT collect_result EQUAL 0)
  message(FATAL_ERROR "收集许可证失败：${collect_output}${collect_error}")
endif()
file(READ "${output}" bundle)
foreach(expected "来源：LICENSE" "来源：a/NOTICE.txt" "来源：b/COPYING" "root license"
                 "notice a" "copying b")
  string(FIND "${bundle}" "${expected}" offset)
  if(offset EQUAL -1)
    message(FATAL_ERROR "许可证汇总缺少内容：${expected}")
  endif()
endforeach()
string(FIND "${bundle}" "must not be collected" unexpected_offset)
if(NOT unexpected_offset EQUAL -1)
  message(FATAL_ERROR "许可证汇总错误包含普通文件")
endif()

