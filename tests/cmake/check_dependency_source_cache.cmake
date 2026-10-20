# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(NOT GRANIT_SOURCE_DIR OR NOT GRANIT_BINARY_DIR)
  message(FATAL_ERROR "测试需要源码及构建目录")
endif()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run)
set(root "${GRANIT_BINARY_DIR}/cmake-tests/source-cache-${run}")
file(MAKE_DIRECTORY "${root}/fixture/demo")
file(WRITE "${root}/fixture/demo/CMakeLists.txt" "# fixture\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar czf "${root}/demo.tar.gz" demo
  WORKING_DIRECTORY "${root}/fixture"
  RESULT_VARIABLE packed
)
if(NOT packed EQUAL 0)
  message(FATAL_ERROR "创建测试归档失败")
endif()
file(SHA256 "${root}/demo.tar.gz" hash)
set(archive "${root}/demo.tar.gz")
set(HASH "${hash}")

set(child_template [=[
include("@GRANIT_SOURCE_DIR@/cmake/granit_dependency_cache.cmake")
set(GRANIT_DEPENDENCY_SOURCE_CACHE_DIR "@cache@")
granit_prepare_source_cache(demo v1 "file://@archive@" "@HASH@" CMakeLists.txt source)
file(WRITE "@root@/@result@" "${source}")
]=])

function(write_child cache result)
  string(CONFIGURE "${child_template}" child @ONLY)
  file(WRITE "${root}/child-${result}.cmake" "${child}")
endfunction()

function(run_child cache id expected)
  write_child("${cache}" "${id}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DGRANIT_DEPENDENCY_OFFLINE=${GRANIT_DEPENDENCY_OFFLINE}"
      -P "${root}/child-${id}.cmake"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error
  )
  if(expected STREQUAL "success" AND NOT status EQUAL 0)
    message(FATAL_ERROR "缓存应成功：${output} ${error}")
  elseif(expected STREQUAL "failure" AND status EQUAL 0)
    message(FATAL_ERROR "缓存应拒绝无效输入")
  endif()
endfunction()

set(cache "${root}/cache")
run_child("${cache}" single success)
file(READ "${root}/single" first)
run_child("${cache}" reuse success)
file(READ "${root}/reuse" second)
if(NOT first STREQUAL second OR NOT EXISTS "${first}/.granit-source-complete")
  message(FATAL_ERROR "缓存命中目录或完成标记错误")
endif()

file(MAKE_DIRECTORY "${root}/archive-only/archives")
file(COPY_FILE "${root}/demo.tar.gz" "${root}/archive-only/archives/demo-v1.tar.gz")
set(GRANIT_DEPENDENCY_OFFLINE ON)
run_child("${root}/archive-only" offline success)

file(MAKE_DIRECTORY "${root}/corrupt/archives")
file(WRITE "${root}/corrupt/archives/demo-v1.tar.gz" "corrupt")
run_child("${root}/corrupt" corrupt failure)

# execute_process 的多个 COMMAND 用于并发填充同一个缓存项；两者输出到独立文件。
write_child("${root}/parallel" a)
write_child("${root}/parallel" b)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -P "${root}/child-a.cmake"
  COMMAND "${CMAKE_COMMAND}" -P "${root}/child-b.cmake"
  RESULTS_VARIABLE statuses ERROR_VARIABLE errors
)
if(NOT statuses STREQUAL "0;0")
  message(FATAL_ERROR "并发缓存失败：${statuses} ${errors}")
endif()
file(READ "${root}/a" parallel_a)
file(READ "${root}/b" parallel_b)
if(NOT parallel_a STREQUAL parallel_b OR NOT EXISTS "${parallel_a}/.granit-source-complete")
  message(FATAL_ERROR "并发未发布同一份完整源码缓存")
endif()

message(STATUS "源码缓存复用、离线、损坏拒绝和并发填充测试通过")
