# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR)
  message(FATAL_ERROR "必须通过 GRANIT_SOURCE_DIR 指定仓库根目录")
endif()

set(granit_include_root "${GRANIT_SOURCE_DIR}/include/granit")
file(
  GLOB_RECURSE granit_public_headers
  LIST_DIRECTORIES FALSE
  "${granit_include_root}/*.h"
  "${granit_include_root}/*.hpp"
)

set(granit_long_term_count 0)
set(granit_observation_count 0)
set(granit_experimental_count 0)
set(granit_unclassified_headers)

foreach(granit_header IN LISTS granit_public_headers)
  file(RELATIVE_PATH granit_relative_header "${granit_include_root}" "${granit_header}")
  string(REPLACE "\\" "/" granit_relative_header "${granit_relative_header}")

  if(granit_relative_header MATCHES "^(granit\\.(h|hpp)|window\\.(h|hpp))$" OR
     granit_relative_header MATCHES "^(core|math|renderer|window)/")
    math(EXPR granit_long_term_count "${granit_long_term_count} + 1")
  elseif(granit_relative_header MATCHES "^pipeline/")
    math(EXPR granit_observation_count "${granit_observation_count} + 1")
  elseif(granit_relative_header MATCHES "^(asset_tools|integrations)/")
    math(EXPR granit_experimental_count "${granit_experimental_count} + 1")
  else()
    list(APPEND granit_unclassified_headers "${granit_relative_header}")
  endif()
endforeach()

if(granit_unclassified_headers)
  list(JOIN granit_unclassified_headers "\n  - " granit_unclassified_text)
  message(FATAL_ERROR "发现未分类的安装公共头：\n  - ${granit_unclassified_text}")
endif()

foreach(granit_required_count IN ITEMS granit_long_term_count granit_observation_count
                                       granit_experimental_count)
  if(${granit_required_count} EQUAL 0)
    message(FATAL_ERROR "公共 API 稳定等级分类为空：${granit_required_count}")
  endif()
endforeach()

message(
  STATUS
  "公共头稳定等级检查通过：长期候选 ${granit_long_term_count}，观察候选 "
  "${granit_observation_count}，实验性 ${granit_experimental_count}"
)
