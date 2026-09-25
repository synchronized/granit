# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_TEST_BINARY_DIR)
  message(FATAL_ERROR "缺少 GRANIT_SOURCE_DIR 或 GRANIT_TEST_BINARY_DIR")
endif()

set(fixture_dir "${GRANIT_TEST_BINARY_DIR}/prepare-release")
file(REMOVE_RECURSE "${fixture_dir}")
file(MAKE_DIRECTORY "${fixture_dir}/cmake")
file(WRITE "${fixture_dir}/cmake/granit_version.cmake"
     "set(GRANIT_PROJECT_VERSION \"1.2.3\")\n")
file(WRITE "${fixture_dir}/README.md"
     "> **最新发布版本：1.2.3。**\n"
     "\n"
     "历史说明：1.2.3 起只发布共享库 SDK。\n"
     "\n"
     "[Granit 1.2.3 Release](https://github.com/synchronized/granit/releases/tag/v1.2.3)\n")
file(WRITE "${fixture_dir}/CHANGELOG.md" "# Changelog\n\n## Unreleased\n\n### Added\n")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" "-DGRANIT_SOURCE_DIR=${fixture_dir}"
    -DGRANIT_NEW_VERSION=1.3.0 -DGRANIT_RELEASE_DATE=2026-09-25
    -P "${GRANIT_SOURCE_DIR}/scripts/prepare_release.cmake"
  RESULT_VARIABLE prepare_result
  OUTPUT_VARIABLE prepare_output
  ERROR_VARIABLE prepare_error
)
if(NOT prepare_result EQUAL 0)
  message(FATAL_ERROR "版本准备失败：\n${prepare_output}\n${prepare_error}")
endif()

file(READ "${fixture_dir}/cmake/granit_version.cmake" version_content)
file(READ "${fixture_dir}/README.md" readme_content)
file(READ "${fixture_dir}/CHANGELOG.md" changelog_content)
if(NOT version_content MATCHES "GRANIT_PROJECT_VERSION \"1\\.3\\.0\"")
  message(FATAL_ERROR "工程版本没有更新")
endif()
if(NOT readme_content MATCHES "最新发布版本：1\\.3\\.0")
  message(FATAL_ERROR "README 当前版本没有更新")
endif()
if(NOT readme_content MATCHES "Granit 1\\.3\\.0 Release.*tag/v1\\.3\\.0")
  message(FATAL_ERROR "README Release 链接没有更新")
endif()
if(NOT readme_content MATCHES "历史说明：1\\.2\\.3 起只发布共享库 SDK")
  message(FATAL_ERROR "README 历史版本文本被错误修改")
endif()
if(NOT changelog_content MATCHES "## 1\\.3\\.0 - 2026-09-25")
  message(FATAL_ERROR "Changelog 版本章节没有插入")
endif()
