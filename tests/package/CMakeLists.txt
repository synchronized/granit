# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_package_selection LANGUAGES C)

set(GRANIT_REQUEST_VERSION "0.1" CACHE STRING "请求的 Granit 包版本")
set(GRANIT_REQUEST_COMPONENT "RenderPipeline" CACHE STRING "请求的 Granit component")
option(GRANIT_REQUEST_EXACT "要求精确匹配 Granit 版本" OFF)

if(GRANIT_REQUEST_EXACT)
  find_package(
    granit ${GRANIT_REQUEST_VERSION} EXACT CONFIG REQUIRED
    COMPONENTS ${GRANIT_REQUEST_COMPONENT}
  )
else()
  find_package(
    granit ${GRANIT_REQUEST_VERSION} CONFIG REQUIRED
    COMPONENTS ${GRANIT_REQUEST_COMPONENT}
  )
endif()

if(NOT TARGET granit::granit)
  message(FATAL_ERROR "Granit 核心导入目标缺失")
endif()
if(GRANIT_REQUEST_COMPONENT STREQUAL "RenderPipeline" AND NOT TARGET granit::render_pipeline)
  message(FATAL_ERROR "Granit RenderPipeline 导入目标缺失")
endif()
