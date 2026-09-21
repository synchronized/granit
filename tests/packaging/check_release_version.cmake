# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR OR NOT DEFINED GRANIT_RELEASE_TAG)
  message(FATAL_ERROR "必须提供 GRANIT_SOURCE_DIR 和 GRANIT_RELEASE_TAG")
endif()

include("${GRANIT_SOURCE_DIR}/cmake/granit_version.cmake")

set(granit_expected_tag "v${GRANIT_PROJECT_VERSION}")
if(NOT GRANIT_RELEASE_TAG STREQUAL granit_expected_tag)
  message(FATAL_ERROR
    "发布标签 ${GRANIT_RELEASE_TAG} 与项目版本 ${GRANIT_PROJECT_VERSION} 不一致，"
    "预期 ${granit_expected_tag}"
  )
endif()

message(STATUS "发布身份验证通过：${GRANIT_RELEASE_TAG}")
