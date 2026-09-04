# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT DEFINED GRANIT_SOURCE_DIR)
  message(FATAL_ERROR "必须通过 GRANIT_SOURCE_DIR 指定仓库根目录")
endif()

cmake_path(ABSOLUTE_PATH GRANIT_SOURCE_DIR NORMALIZE OUTPUT_VARIABLE granit_docs_root)

set(
  granit_docs_files
  "${granit_docs_root}/README.md"
  "${granit_docs_root}/DOCUMENTATION_GUIDE.md"
)
file(GLOB_RECURSE granit_docs_tree LIST_DIRECTORIES FALSE "${granit_docs_root}/docs/*.md")
list(APPEND granit_docs_files ${granit_docs_tree})

set(granit_docs_errors)
foreach(granit_doc IN LISTS granit_docs_files)
  file(READ "${granit_doc}" granit_doc_content)
  string(
    REGEX MATCHALL
    "\\[[^]\r\n]*\\]\\([^) \t\r\n]+\\)"
    granit_doc_links
    "${granit_doc_content}"
  )
  get_filename_component(granit_doc_directory "${granit_doc}" DIRECTORY)

  foreach(granit_doc_link IN LISTS granit_doc_links)
    string(
      REGEX REPLACE
      "^.*\\]\\(([^) \t\r\n]+)\\)$"
      "\\1"
      granit_doc_target
      "${granit_doc_link}"
    )
    if(granit_doc_target MATCHES "^(https?://|mailto:|#)")
      continue()
    endif()

    string(REGEX REPLACE "#.*$" "" granit_doc_path "${granit_doc_target}")
    if(granit_doc_path STREQUAL "")
      continue()
    endif()

    cmake_path(
      ABSOLUTE_PATH granit_doc_path
      BASE_DIRECTORY "${granit_doc_directory}"
      NORMALIZE
      OUTPUT_VARIABLE granit_doc_resolved
    )
    if(NOT EXISTS "${granit_doc_resolved}")
      file(RELATIVE_PATH granit_doc_relative "${granit_docs_root}" "${granit_doc}")
      list(APPEND granit_docs_errors "${granit_doc_relative}: ${granit_doc_target}")
    endif()
  endforeach()
endforeach()

file(READ "${granit_docs_root}/README.md" granit_root_readme)

function(granit_require_document_text relative_path expected_text description)
  file(READ "${granit_docs_root}/${relative_path}" granit_checked_document)
  string(FIND "${granit_checked_document}" "${expected_text}" granit_text_position)
  if(granit_text_position EQUAL -1)
    set_property(
      GLOBAL APPEND PROPERTY GRANIT_DOCUMENTATION_COMMAND_ERRORS
      "${relative_path}: 缺少${description}"
    )
  endif()
endfunction()

set_property(GLOBAL PROPERTY GRANIT_DOCUMENTATION_COMMAND_ERRORS "")
granit_require_document_text(
  "README.md" "find_package(granit CONFIG REQUIRED)" "核心安装包 CMake 入口"
)
granit_require_document_text(
  "README.md" "COMPONENTS RenderPipeline" "RenderPipeline component 入口"
)
granit_require_document_text(
  "docs/guides/build.md"
  "ctest --test-dir build/consumer --output-on-failure"
  "独立安装 Consumer 执行命令"
)
granit_require_document_text(
  "docs/tutorials/render-pipeline-offscreen.md"
  "granit.smoke.render_pipeline_offscreen"
  "RenderPipeline 教程验证命令"
)
get_property(granit_command_errors GLOBAL PROPERTY GRANIT_DOCUMENTATION_COMMAND_ERRORS)
if(granit_command_errors)
  list(APPEND granit_docs_errors ${granit_command_errors})
endif()

string(REGEX MATCHALL "\n" granit_root_readme_newlines "${granit_root_readme}")
list(LENGTH granit_root_readme_newlines granit_root_readme_line_count)
math(EXPR granit_root_readme_line_count "${granit_root_readme_line_count} + 1")
if(granit_root_readme_line_count GREATER 180)
  list(
    APPEND granit_docs_errors
    "README.md: ${granit_root_readme_line_count} 行，超过 180 行软上限"
  )
endif()

function(granit_check_document_index directory index_file prefix)
  file(READ "${index_file}" granit_index_content)
  file(GLOB granit_indexed_docs LIST_DIRECTORIES FALSE "${directory}/*.md")
  foreach(granit_indexed_doc IN LISTS granit_indexed_docs)
    get_filename_component(granit_indexed_name "${granit_indexed_doc}" NAME)
    if(granit_indexed_name STREQUAL "README.md")
      continue()
    endif()
    set(granit_expected_target "${prefix}${granit_indexed_name}")
    string(FIND "${granit_index_content}" "](${granit_expected_target})" granit_index_position)
    if(granit_index_position EQUAL -1)
      file(RELATIVE_PATH granit_unindexed_relative "${granit_docs_root}" "${granit_indexed_doc}")
      set_property(
        GLOBAL APPEND PROPERTY GRANIT_DOCUMENTATION_INDEX_ERRORS
        "${granit_unindexed_relative}: 未加入对应 README 索引"
      )
    endif()
  endforeach()
endfunction()

set_property(GLOBAL PROPERTY GRANIT_DOCUMENTATION_INDEX_ERRORS "")
foreach(granit_category IN ITEMS guides reference concepts)
  granit_check_document_index(
    "${granit_docs_root}/docs/${granit_category}"
    "${granit_docs_root}/docs/README.md"
    "${granit_category}/"
  )
endforeach()
granit_check_document_index(
  "${granit_docs_root}/docs/plans"
  "${granit_docs_root}/docs/plans/README.md"
  ""
)
granit_check_document_index(
  "${granit_docs_root}/docs/records"
  "${granit_docs_root}/docs/records/README.md"
  ""
)
get_property(granit_index_errors GLOBAL PROPERTY GRANIT_DOCUMENTATION_INDEX_ERRORS)
if(granit_index_errors)
  list(APPEND granit_docs_errors ${granit_index_errors})
endif()

if(granit_docs_errors)
  list(JOIN granit_docs_errors "\n  - " granit_docs_error_text)
  message(FATAL_ERROR "文档检查失败：\n  - ${granit_docs_error_text}")
endif()

list(LENGTH granit_docs_files granit_docs_file_count)
message(
  STATUS
  "文档检查通过：${granit_docs_file_count} 个 Markdown 文件，根 README "
  "${granit_root_readme_line_count} 行"
)
