# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

# 将锁定版本的源码保存到仓库级缓存。源码目录可被多个 preset 只读复用，目标和中间文件仍由
# 具体依赖的 CMake 目标放在当前构建目录中，避免不同编译器、平台和链接模式互相污染。
# 同一缓存项使用进程锁和临时 staging 目录，防止并行 configure 读取半成品。
function(granit_prepare_source_cache name commit url hash marker out_source_dir)
  if(NOT name OR NOT commit OR NOT url OR NOT hash OR NOT marker OR NOT out_source_dir)
    message(FATAL_ERROR "源码缓存准备需要 name、commit、url、hash、marker 和输出变量")
  endif()
  set(cache_root "${GRANIT_DEPENDENCY_SOURCE_CACHE_DIR}")
  set(archive_dir "${cache_root}/archives")
  set(source_dir "${cache_root}/sources/${name}/${commit}")
  set(archive "${archive_dir}/${name}-${commit}.tar.gz")

  file(MAKE_DIRECTORY "${cache_root}/locks" "${archive_dir}" "${cache_root}/staging"
       "${cache_root}/sources/${name}")
  file(LOCK "${cache_root}/locks/${name}-${commit}.lock"
       GUARD FUNCTION TIMEOUT 300 RESULT_VARIABLE locked)
  if(NOT locked STREQUAL "0")
    message(FATAL_ERROR "依赖 ${name} 缓存锁失败：${cache_root}：${locked}")
  endif()

  if(EXISTS "${source_dir}/.granit-source-complete" AND EXISTS "${source_dir}/${marker}")
    file(READ "${source_dir}/.granit-source-complete" receipt)
    if(receipt STREQUAL "${hash}\n")
      set(${out_source_dir} "${source_dir}" PARENT_SCOPE)
      return()
    endif()
  elseif(EXISTS "${source_dir}/${marker}")
    # 兼容锁机制加入前已经填充的完整缓存；后续访问使用完成标记校验。
    file(WRITE "${source_dir}/.granit-source-complete" "${hash}\n")
    set(${out_source_dir} "${source_dir}" PARENT_SCOPE)
    return()
  endif()
  if(EXISTS "${source_dir}")
    message(FATAL_ERROR "依赖 ${name} 源码缓存不完整，请移走后重试：${source_dir}")
  endif()

  string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef nonce)
  set(stage "${cache_root}/staging/${name}-${commit}-${nonce}")
  file(MAKE_DIRECTORY "${stage}")
  if(EXISTS "${archive}")
    file(SHA256 "${archive}" archive_hash)
    if(NOT archive_hash STREQUAL "${hash}")
      message(FATAL_ERROR "依赖 ${name} 归档摘要不匹配，请移走后重试：${archive}")
    endif()
  else()
    if(GRANIT_DEPENDENCY_OFFLINE)
      message(FATAL_ERROR "离线依赖 ${name} 缓存缺失：${cache_root}")
    endif()
    message(STATUS "正在下载 ${name} 源码到共享缓存：${cache_root}")
    file(DOWNLOAD "${url}" "${stage}/download.tar.gz" TLS_VERIFY ON STATUS download_status
         TIMEOUT 300)
    list(GET download_status 0 download_code)
    if(NOT download_code EQUAL 0)
      message(FATAL_ERROR "依赖 ${name} 下载失败：${download_status}；缓存：${cache_root}")
    endif()
    file(SHA256 "${stage}/download.tar.gz" archive_hash)
    if(NOT archive_hash STREQUAL "${hash}")
      message(FATAL_ERROR "依赖 ${name} 下载摘要不匹配：${stage}")
    endif()
    file(RENAME "${stage}/download.tar.gz" "${archive}")
  endif()

  file(MAKE_DIRECTORY "${stage}/unpack")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xzf "${archive}"
    WORKING_DIRECTORY "${stage}/unpack"
    RESULT_VARIABLE extract_result
    ERROR_VARIABLE extract_error
  )
  if(NOT extract_result EQUAL 0)
    message(FATAL_ERROR "解压 ${name} 共享源码缓存失败：${extract_error}")
  endif()
  file(GLOB extracted_entries LIST_DIRECTORIES TRUE "${stage}/unpack/*")
  list(LENGTH extracted_entries extracted_count)
  if(NOT extracted_count EQUAL 1)
    message(FATAL_ERROR "无法确定 ${name} 归档的源码根目录：${stage}")
  endif()
  list(GET extracted_entries 0 extracted_source_dir)
  if(NOT IS_DIRECTORY "${extracted_source_dir}" OR NOT EXISTS "${extracted_source_dir}/${marker}")
    message(FATAL_ERROR "依赖 ${name} 缺少入口 ${marker}：${stage}")
  endif()
  file(WRITE "${extracted_source_dir}/.granit-source-complete" "${hash}\n")
  file(RENAME "${extracted_source_dir}" "${source_dir}")
  file(REMOVE_RECURSE "${stage}")
  set(${out_source_dir} "${source_dir}" PARENT_SCOPE)
endfunction()
