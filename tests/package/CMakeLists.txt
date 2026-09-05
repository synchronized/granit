# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_package_selection LANGUAGES C)

set(GRANIT_REQUEST_VERSION "0.1" CACHE STRING "请求的 Granit 包版本")
set(GRANIT_REQUEST_COMPONENT "" CACHE STRING "请求的 Granit component；空值只请求 Core")
option(GRANIT_REQUEST_EXACT "要求精确匹配 Granit 版本" OFF)

if(GRANIT_REQUEST_EXACT AND GRANIT_REQUEST_COMPONENT)
  find_package(
    granit ${GRANIT_REQUEST_VERSION} EXACT CONFIG REQUIRED
    COMPONENTS ${GRANIT_REQUEST_COMPONENT}
  )
elseif(GRANIT_REQUEST_EXACT)
  find_package(granit ${GRANIT_REQUEST_VERSION} EXACT CONFIG REQUIRED)
elseif(GRANIT_REQUEST_COMPONENT)
  find_package(
    granit ${GRANIT_REQUEST_VERSION} CONFIG REQUIRED
    COMPONENTS ${GRANIT_REQUEST_COMPONENT}
  )
else()
  find_package(granit ${GRANIT_REQUEST_VERSION} CONFIG REQUIRED)
endif()

if(NOT TARGET granit::granit)
  message(FATAL_ERROR "Granit 核心导入目标缺失")
endif()

if(NOT GRANIT_REQUEST_COMPONENT)
  foreach(
    unexpected_target
    IN ITEMS
      granit::render_pipeline
      granit::window
      granit::input
      granit::shader_tools
      granit::integration_sdl3
      granit::integration_imgui
  )
    if(TARGET ${unexpected_target})
      message(FATAL_ERROR "Core-only 请求意外导入可选目标：${unexpected_target}")
    endif()
  endforeach()
elseif(GRANIT_REQUEST_COMPONENT STREQUAL "RenderPipeline")
  set(expected_target granit::render_pipeline)
elseif(GRANIT_REQUEST_COMPONENT STREQUAL "Window")
  set(expected_target granit::window)
elseif(GRANIT_REQUEST_COMPONENT STREQUAL "Input")
  set(expected_target granit::input)
  if(NOT TARGET granit::window)
    message(FATAL_ERROR "Input component 未导入其 Window 依赖")
  endif()
elseif(GRANIT_REQUEST_COMPONENT STREQUAL "ShaderTools")
  set(expected_target granit::shader_tools)
elseif(GRANIT_REQUEST_COMPONENT STREQUAL "IntegrationSDL3")
  set(expected_target granit::integration_sdl3)
elseif(GRANIT_REQUEST_COMPONENT STREQUAL "IntegrationImGui")
  set(expected_target granit::integration_imgui)
endif()

if(DEFINED expected_target AND NOT TARGET ${expected_target})
  message(FATAL_ERROR
          "Granit ${GRANIT_REQUEST_COMPONENT} 导入目标缺失：${expected_target}")
endif()
