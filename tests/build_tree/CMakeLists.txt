# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_build_tree_consumer LANGUAGES C CXX)

set(GRANIT_SOURCE_DIR "" CACHE PATH "Granit 源码目录")
set(GRANIT_CONSUMER_MODE "add_subdirectory" CACHE STRING "构建树消费方式")
set(GRANIT_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(GRANIT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

if(GRANIT_CONSUMER_MODE STREQUAL "add_subdirectory")
  add_subdirectory("${GRANIT_SOURCE_DIR}" granit)
elseif(GRANIT_CONSUMER_MODE STREQUAL "fetch_content")
  include(FetchContent)
  FetchContent_Declare(granit SOURCE_DIR "${GRANIT_SOURCE_DIR}")
  FetchContent_MakeAvailable(granit)
else()
  message(FATAL_ERROR "未知构建树消费方式：${GRANIT_CONSUMER_MODE}")
endif()

if(NOT TARGET granit::render_pipeline)
  message(FATAL_ERROR "构建树缺少 granit::render_pipeline")
endif()
foreach(asset IN ITEMS
    libraries/pbr_standard.grshlib
    materials/pbr_standard.grmat
    environments/studio_small_03.grenv)
  if(NOT EXISTS "${granit_RENDER_PIPELINE_ASSET_DIR}/${asset}")
    message(FATAL_ERROR "构建树 RenderPipeline 缺少资产：${asset}")
  endif()
endforeach()
file(GLOB_RECURSE legacy_shader_assets
     "${granit_RENDER_PIPELINE_ASSET_DIR}/*.grshaderobj"
     "${granit_RENDER_PIPELINE_ASSET_DIR}/*.grshaderobj.*")
if(legacy_shader_assets)
  message(FATAL_ERROR "构建树 RenderPipeline 仍公开旧 Shader Asset：${legacy_shader_assets}")
endif()
