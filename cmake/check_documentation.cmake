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
file(READ "${granit_docs_root}/CMakeLists.txt" granit_root_cmake)
string(
  REGEX MATCH
  "project\\([ \t\r\n]*granit[ \t\r\n]+VERSION[ \t\r\n]+([0-9]+\\.[0-9]+\\.[0-9]+)"
  granit_project_match
  "${granit_root_cmake}"
)
if(NOT granit_project_match)
  list(APPEND granit_docs_errors "CMakeLists.txt: 无法读取 Granit 项目版本")
else()
  set(granit_project_version "${CMAKE_MATCH_1}")
endif()

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
  "granit.gpu.render_pipeline"
  "RenderPipeline 教程验证命令"
)
if(granit_project_version)
  granit_require_document_text(
    "README.md"
    "最新发布版本：${granit_project_version}"
    "与项目版本一致的最新发布版本"
  )
  granit_require_document_text(
    "README.md"
    "releases/tag/v${granit_project_version}"
    "与项目版本一致的 Release 下载入口"
  )
  granit_require_document_text(
    "CHANGELOG.md"
    "## ${granit_project_version} -"
    "当前项目版本 Changelog 标题"
  )
endif()
get_property(granit_command_errors GLOBAL PROPERTY GRANIT_DOCUMENTATION_COMMAND_ERRORS)
if(granit_command_errors)
  list(APPEND granit_docs_errors ${granit_command_errors})
endif()

set(granit_current_contract_docs "${granit_docs_root}/README.md")
foreach(granit_contract_category IN ITEMS concepts reference tutorials)
  file(
    GLOB granit_contract_category_docs
    LIST_DIRECTORIES FALSE
    "${granit_docs_root}/docs/${granit_contract_category}/*.md"
  )
  list(APPEND granit_current_contract_docs ${granit_contract_category_docs})
endforeach()
file(
  GLOB granit_current_guides
  LIST_DIRECTORIES FALSE
  "${granit_docs_root}/docs/guides/*.md"
)
foreach(granit_current_guide IN LISTS granit_current_guides)
  get_filename_component(granit_current_guide_name "${granit_current_guide}" NAME)
  if(NOT granit_current_guide_name MATCHES "^migrate-")
    list(APPEND granit_current_contract_docs "${granit_current_guide}")
  endif()
endforeach()

set(
  granit_removed_input_contracts
  "`granit::input`"
  "<granit/input"
  "granit_input_system_create"
  "COMPONENTS Input"
)
foreach(granit_contract_doc IN LISTS granit_current_contract_docs)
  file(READ "${granit_contract_doc}" granit_contract_content)
  foreach(granit_removed_contract IN LISTS granit_removed_input_contracts)
    string(FIND "${granit_contract_content}" "${granit_removed_contract}" granit_contract_position)
    if(NOT granit_contract_position EQUAL -1)
      file(RELATIVE_PATH granit_contract_relative "${granit_docs_root}" "${granit_contract_doc}")
      list(
        APPEND granit_docs_errors
        "${granit_contract_relative}: 当前文档仍引用已删除的 Input component 契约 ${granit_removed_contract}"
      )
    endif()
  endforeach()
endforeach()

file(
  READ
  "${granit_docs_root}/docs/plans/S-42-0.24.0-test-architecture-convergence.md"
  granit_s42_plan
)
string(FIND "${granit_s42_plan}" "等待远端验收" granit_s42_pending_position)
if(NOT granit_s42_pending_position EQUAL -1)
  list(APPEND granit_docs_errors "S-42: 已发布计划仍标记为等待远端验收")
endif()

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${granit_docs_root}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" tag --list "v[0-9]*"
    WORKING_DIRECTORY "${granit_docs_root}"
    RESULT_VARIABLE granit_git_tags_result
    OUTPUT_VARIABLE granit_git_tags
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(granit_git_tags_result EQUAL 0 AND NOT granit_git_tags STREQUAL "")
    file(READ "${granit_docs_root}/CHANGELOG.md" granit_changelog)
    string(REPLACE "\r\n" "\n" granit_git_tags "${granit_git_tags}")
    string(REPLACE "\n" ";" granit_git_tag_list "${granit_git_tags}")
    foreach(granit_git_tag IN LISTS granit_git_tag_list)
      if(granit_git_tag MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)$")
        set(granit_tag_version "${CMAKE_MATCH_1}")
        string(FIND "${granit_changelog}" "## ${granit_tag_version} -" granit_tag_heading_position)
        if(granit_tag_heading_position EQUAL -1)
          list(APPEND granit_docs_errors "CHANGELOG.md: 缺少已发布标签 ${granit_git_tag}")
        endif()
      endif()
    endforeach()
  endif()
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

function(granit_check_guide_index directory main_index migration_index)
  file(READ "${main_index}" granit_main_index_content)
  file(READ "${migration_index}" granit_migration_index_content)
  file(GLOB granit_guide_docs LIST_DIRECTORIES FALSE "${directory}/*.md")
  foreach(granit_guide_doc IN LISTS granit_guide_docs)
    get_filename_component(granit_guide_name "${granit_guide_doc}" NAME)
    if(granit_guide_name MATCHES "^migrate-")
      set(granit_expected_target "${granit_guide_name}")
      set(granit_index_content "${granit_migration_index_content}")
      set(granit_index_description "迁移指南索引")
    else()
      set(granit_expected_target "guides/${granit_guide_name}")
      set(granit_index_content "${granit_main_index_content}")
      set(granit_index_description "文档中心")
    endif()
    string(FIND "${granit_index_content}" "](${granit_expected_target})" granit_index_position)
    if(granit_index_position EQUAL -1)
      file(RELATIVE_PATH granit_unindexed_relative "${granit_docs_root}" "${granit_guide_doc}")
      set_property(
        GLOBAL APPEND PROPERTY GRANIT_DOCUMENTATION_INDEX_ERRORS
        "${granit_unindexed_relative}: 未加入${granit_index_description}"
      )
    endif()
  endforeach()
endfunction()

set_property(GLOBAL PROPERTY GRANIT_DOCUMENTATION_INDEX_ERRORS "")
granit_check_guide_index(
  "${granit_docs_root}/docs/guides"
  "${granit_docs_root}/docs/README.md"
  "${granit_docs_root}/docs/guides/migrations.md"
)
foreach(granit_category IN ITEMS reference concepts)
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
