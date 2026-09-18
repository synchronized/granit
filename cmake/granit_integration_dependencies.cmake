# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

set(GRANIT_SDL3_LOCKED_VERSION "3.4.10")
set(GRANIT_IMGUI_LOCKED_VERSION "1.92.9")

set(GRANIT_DEPENDENCY_POLICY_VALUES system auto download)

# 解析某依赖的有效获取策略：每依赖覆盖（GRANIT_DEPENDENCY_<DEP>）优先，
# 为空则回落到全局 GRANIT_DEPENDENCY_POLICY。
function(granit_dependency_policy dep out_policy)
  set(policy "${GRANIT_DEPENDENCY_${dep}}")
  if(NOT policy)
    set(policy "${GRANIT_DEPENDENCY_POLICY}")
  endif()
  if(NOT policy IN_LIST GRANIT_DEPENDENCY_POLICY_VALUES)
    message(FATAL_ERROR
      "GRANIT_DEPENDENCY_${dep}='${policy}' 无效。允许值：${GRANIT_DEPENDENCY_POLICY_VALUES}")
  endif()
  set(${out_policy} "${policy}" PARENT_SCOPE)
endfunction()

# 下载锁定版本 SDL3。
function(granit_fetch_sdl3)
  include(FetchContent)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    granit_sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG "release-${GRANIT_SDL3_LOCKED_VERSION}"
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(granit_sdl3)
  set(GRANIT_SDL3_FETCHED TRUE PARENT_SCOPE)
  set(granit_sdl3_SOURCE_DIR "${granit_sdl3_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

# 下载锁定版本 ImGui 并封装为统一目标。
function(granit_fetch_imgui)
  include(FetchContent)
  FetchContent_Declare(
    granit_imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG "v${GRANIT_IMGUI_LOCKED_VERSION}"
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(granit_imgui)
  add_library(
    granit_imgui_dependency STATIC
    "${granit_imgui_SOURCE_DIR}/imgui.cpp"
    "${granit_imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${granit_imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${granit_imgui_SOURCE_DIR}/imgui_widgets.cpp"
  )
  target_include_directories(granit_imgui_dependency PUBLIC "${granit_imgui_SOURCE_DIR}")
  target_compile_features(granit_imgui_dependency PUBLIC cxx_std_20)
  set_target_properties(
    granit_imgui_dependency PROPERTIES POSITION_INDEPENDENT_CODE YES FOLDER "Third Party"
  )
  set(GRANIT_IMGUI_FETCHED TRUE PARENT_SCOPE)
  set(granit_imgui_SOURCE_DIR "${granit_imgui_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

# 准备 SDL3 集成依赖。可选参数为策略覆盖（system/auto/download），
# 省略时按 GRANIT_DEPENDENCY_SDL3 / GRANIT_DEPENDENCY_POLICY 解析。
function(granit_prepare_sdl3_dependency)
  if(ARGC GREATER 0 AND ARGV0)
    set(policy "${ARGV0}")
  else()
    granit_dependency_policy(SDL3 policy)
  endif()

  if(policy STREQUAL "download")
    granit_fetch_sdl3()
  else()
    find_package(SDL3 3.2 CONFIG QUIET)
    if(NOT TARGET SDL3::SDL3 AND policy STREQUAL "auto")
      granit_fetch_sdl3()
    endif()
  endif()

  if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR
      "已启用 SDL3 Integration，但未找到 SDL3::SDL3（最低版本 3.2）；"
      "当前获取策略为 ${policy}，请提供系统包或调整 GRANIT_DEPENDENCY_POLICY")
  endif()
  set(GRANIT_SDL3_TARGET SDL3::SDL3 PARENT_SCOPE)
endfunction()

# 准备 ImGui 集成依赖。可选参数为策略覆盖，语义同 granit_prepare_sdl3_dependency。
function(granit_prepare_imgui_dependency)
  if(ARGC GREATER 0 AND ARGV0)
    set(policy "${ARGV0}")
  else()
    granit_dependency_policy(IMGUI policy)
  endif()

  if(policy STREQUAL "download")
    granit_fetch_imgui()
  else()
    find_package(imgui CONFIG QUIET)
    if(NOT TARGET imgui::imgui AND NOT TARGET ImGui::ImGui AND NOT TARGET imgui
       AND policy STREQUAL "auto")
      granit_fetch_imgui()
    endif()
  endif()

  if(TARGET granit_imgui_dependency)
    set(GRANIT_IMGUI_TARGET granit_imgui_dependency PARENT_SCOPE)
  elseif(TARGET imgui::imgui)
    set(GRANIT_IMGUI_TARGET imgui::imgui PARENT_SCOPE)
  elseif(TARGET ImGui::ImGui)
    set(GRANIT_IMGUI_TARGET ImGui::ImGui PARENT_SCOPE)
  elseif(TARGET imgui)
    set(GRANIT_IMGUI_TARGET imgui PARENT_SCOPE)
  else()
    message(FATAL_ERROR "已启用 ImGui Integration，但未找到可用的 ImGui 目标")
  endif()
endfunction()
