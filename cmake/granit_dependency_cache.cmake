# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

# 将锁定版本的源码保存到仓库级缓存。源码目录可被多个 preset 只读复用，目标和中间文件仍由
# 具体依赖的 CMake 目标放在当前构建目录中，避免不同编译器、平台和链接模式互相污染。
function(granit_prepare_source_cache name commit url hash marker out_source_dir)
  if(NOT name OR NOT commit OR NOT url OR NOT hash OR NOT marker OR NOT out_source_dir)
    message(FATAL_ERROR "源码缓存准备需要 name、commit、url、hash、marker 和输出变量")
  endif()
  set(cache_root "${GRANIT_DEPENDENCY_SOURCE_CACHE_DIR}")
  set(archive_dir "${cache_root}/archives")
  set(extract_dir "${cache_root}/extract/${name}-${commit}")
  set(source_dir "${cache_root}/sources/${name}/${commit}")
  set(archive "${archive_dir}/${name}-${commit}.tar.gz")

  if(NOT EXISTS "${source_dir}/${marker}")
    file(REMOVE_RECURSE "${extract_dir}")
    file(MAKE_DIRECTORY "${archive_dir}" "${extract_dir}")
    set(archive_valid FALSE)
    if(EXISTS "${archive}")
      file(SHA256 "${archive}" archive_hash)
      if(archive_hash STREQUAL "${hash}")
        set(archive_valid TRUE)
      else()
        file(REMOVE "${archive}")
      endif()
    endif()
    if(NOT archive_valid)
      message(STATUS "正在下载 ${name} 源码到共享缓存：${cache_root}")
      file(DOWNLOAD "${url}" "${archive}" EXPECTED_HASH "SHA256=${hash}" TLS_VERIFY ON)
    endif()
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E tar xzf "${archive}"
      WORKING_DIRECTORY "${extract_dir}"
      RESULT_VARIABLE extract_result
      ERROR_VARIABLE extract_error
    )
    if(NOT extract_result EQUAL 0)
      message(FATAL_ERROR "解压 ${name} 共享源码缓存失败：${extract_error}")
    endif()
    file(GLOB extracted_entries LIST_DIRECTORIES TRUE "${extract_dir}/*")
    list(LENGTH extracted_entries extracted_count)
    if(NOT extracted_count EQUAL 1)
      message(FATAL_ERROR "无法确定 ${name} 归档的源码根目录：${extract_dir}")
    endif()
    list(GET extracted_entries 0 extracted_source_dir)
    file(MAKE_DIRECTORY "${cache_root}/sources/${name}")
    file(RENAME "${extracted_source_dir}" "${source_dir}")
    file(REMOVE_RECURSE "${extract_dir}")
  endif()
  set(${out_source_dir} "${source_dir}" PARENT_SCOPE)
endfunction()
