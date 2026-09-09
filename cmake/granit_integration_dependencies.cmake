# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

set(GRANIT_SDL3_LOCKED_VERSION "3.4.10")
set(GRANIT_IMGUI_LOCKED_VERSION "1.92.9")

function(granit_prepare_sdl3_dependency force_fetch)
  if(NOT TARGET SDL3::SDL3 AND NOT force_fetch)
    find_package(SDL3 3.2 CONFIG QUIET)
  endif()
  if(NOT TARGET SDL3::SDL3 AND (force_fetch OR GRANIT_FETCH_INTEGRATION_DEPENDENCIES))
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
  endif()
  if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR "已启用 SDL3 Integration，但未找到 SDL3::SDL3（最低版本 3.2）")
  endif()
  set(GRANIT_SDL3_TARGET SDL3::SDL3 PARENT_SCOPE)
endfunction()

function(granit_prepare_imgui_dependency force_fetch)
  if(NOT TARGET imgui::imgui AND NOT TARGET ImGui::ImGui AND NOT TARGET imgui AND
     NOT force_fetch)
    find_package(imgui CONFIG QUIET)
  endif()
  if(NOT TARGET imgui::imgui AND NOT TARGET ImGui::ImGui AND NOT TARGET imgui AND
     (force_fetch OR GRANIT_FETCH_INTEGRATION_DEPENDENCIES))
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
