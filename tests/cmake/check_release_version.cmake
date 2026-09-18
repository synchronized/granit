# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_RELEASE_TAG)
  message(FATAL_ERROR "必须提供 GRANIT_SOURCE_DIR 和 GRANIT_RELEASE_TAG")
endif()

file(READ "${GRANIT_SOURCE_DIR}/CMakeLists.txt" granit_root_cmake)
string(
  REGEX MATCH
  "project\\([ \t\r\n]*granit[ \t\r\n]+VERSION[ \t\r\n]+([0-9]+\\.[0-9]+\\.[0-9]+)"
  granit_project_match
  "${granit_root_cmake}"
)
set(granit_project_version "${CMAKE_MATCH_1}")
if(NOT granit_project_match)
  message(FATAL_ERROR "无法从根 CMakeLists.txt 读取 Granit 版本")
endif()

set(granit_expected_tag "v${granit_project_version}")
if(NOT GRANIT_RELEASE_TAG STREQUAL granit_expected_tag)
  message(FATAL_ERROR
    "发布标签 ${GRANIT_RELEASE_TAG} 与项目版本 ${granit_project_version} 不一致，"
    "预期 ${granit_expected_tag}"
  )
endif()

message(STATUS "发布身份验证通过：${GRANIT_RELEASE_TAG}")
