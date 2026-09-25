# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)

foreach(required_variable GRANIT_SOURCE_DIR GRANIT_NEW_VERSION GRANIT_RELEASE_DATE)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "缺少 ${required_variable}")
  endif()
endforeach()

if(NOT GRANIT_NEW_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "版本号格式应为 x.y.z：${GRANIT_NEW_VERSION}")
endif()
if(NOT GRANIT_RELEASE_DATE MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$")
  message(FATAL_ERROR "发布日期格式应为 YYYY-MM-DD：${GRANIT_RELEASE_DATE}")
endif()

set(version_file "${GRANIT_SOURCE_DIR}/cmake/granit_version.cmake")
set(readme_file "${GRANIT_SOURCE_DIR}/README.md")
set(changelog_file "${GRANIT_SOURCE_DIR}/CHANGELOG.md")
foreach(required_file "${version_file}" "${readme_file}" "${changelog_file}")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "缺少发布文件：${required_file}")
  endif()
endforeach()

function(granit_replace_unique_literal variable_name old_text new_text description)
  set(content "${${variable_name}}")
  string(FIND "${content}" "${old_text}" first_position)
  if(first_position EQUAL -1)
    message(FATAL_ERROR "找不到唯一的${description}锚点：${old_text}")
  endif()
  string(LENGTH "${old_text}" old_length)
  math(EXPR remaining_position "${first_position} + ${old_length}")
  string(SUBSTRING "${content}" ${remaining_position} -1 remaining_content)
  string(FIND "${remaining_content}" "${old_text}" second_position)
  if(NOT second_position EQUAL -1)
    message(FATAL_ERROR "${description}锚点出现多次：${old_text}")
  endif()
  string(REPLACE "${old_text}" "${new_text}" content "${content}")
  set(${variable_name} "${content}" PARENT_SCOPE)
endfunction()

file(READ "${version_file}" version_content)
string(REGEX MATCH
       "set\\(GRANIT_PROJECT_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"\\)"
       version_match "${version_content}")
if(NOT version_match)
  message(FATAL_ERROR "无法从 ${version_file} 读取当前版本")
endif()
set(old_version "${CMAKE_MATCH_1}")
if(old_version STREQUAL GRANIT_NEW_VERSION)
  message(FATAL_ERROR "新版本与当前版本相同：${GRANIT_NEW_VERSION}")
endif()

set(old_version_line "set(GRANIT_PROJECT_VERSION \"${old_version}\")")
set(new_version_line "set(GRANIT_PROJECT_VERSION \"${GRANIT_NEW_VERSION}\")")
granit_replace_unique_literal(version_content "${old_version_line}" "${new_version_line}"
                              "工程版本")

file(READ "${readme_file}" readme_content)
set(old_latest "> **最新发布版本：${old_version}。**")
set(new_latest "> **最新发布版本：${GRANIT_NEW_VERSION}。**")
granit_replace_unique_literal(readme_content "${old_latest}" "${new_latest}" "README 最新版本")
set(old_release
    "[Granit ${old_version} Release](https://github.com/synchronized/granit/releases/tag/v${old_version})")
set(new_release
    "[Granit ${GRANIT_NEW_VERSION} Release](https://github.com/synchronized/granit/releases/tag/v${GRANIT_NEW_VERSION})")
granit_replace_unique_literal(readme_content "${old_release}" "${new_release}"
                              "README Release 链接")

file(READ "${changelog_file}" changelog_content)
set(unreleased_heading "## Unreleased")
set(release_headings "## Unreleased\n\n## ${GRANIT_NEW_VERSION} - ${GRANIT_RELEASE_DATE}")
granit_replace_unique_literal(changelog_content "${unreleased_heading}" "${release_headings}"
                              "Changelog Unreleased")

# 所有锚点验证完成后再写文件，避免失败留下部分版本改动。
file(WRITE "${version_file}" "${version_content}")
file(WRITE "${readme_file}" "${readme_content}")
file(WRITE "${changelog_file}" "${changelog_content}")
message(STATUS "已准备 Granit ${old_version} -> ${GRANIT_NEW_VERSION}")
