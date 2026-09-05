# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

foreach(required ROOT OUTPUT COMPONENT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "缺少 ${required}")
  endif()
endforeach()

cmake_path(ABSOLUTE_PATH ROOT NORMALIZE OUTPUT_VARIABLE root_absolute)
cmake_path(ABSOLUTE_PATH OUTPUT NORMALIZE OUTPUT_VARIABLE output_absolute)
if(NOT IS_DIRECTORY "${root_absolute}")
  message(FATAL_ERROR "许可证来源目录不存在：${root_absolute}")
endif()

file(GLOB_RECURSE candidates LIST_DIRECTORIES false RELATIVE "${root_absolute}"
     "${root_absolute}/LICENSE" "${root_absolute}/LICENSE.*" "${root_absolute}/COPYING"
     "${root_absolute}/COPYING.*" "${root_absolute}/NOTICE" "${root_absolute}/NOTICE.*")
list(SORT candidates)
if(NOT candidates)
  message(FATAL_ERROR "未找到可收集的许可证文件：${root_absolute}")
endif()

set(bundle "${COMPONENT} 许可证与第三方声明\n")
string(APPEND bundle "本文件由 Granit 组包流程按来源相对路径确定性生成。\n")
foreach(relative_path IN LISTS candidates)
  file(READ "${root_absolute}/${relative_path}" license_text)
  string(APPEND bundle
         "\n===============================================================================\n"
         "来源：${relative_path}\n"
         "===============================================================================\n"
         "${license_text}")
  if(NOT license_text MATCHES "\n$")
    string(APPEND bundle "\n")
  endif()
endforeach()

cmake_path(GET output_absolute PARENT_PATH output_directory)
file(MAKE_DIRECTORY "${output_directory}")
set(temporary "${output_absolute}.tmp")
file(WRITE "${temporary}" "${bundle}")
file(RENAME "${temporary}" "${output_absolute}")

