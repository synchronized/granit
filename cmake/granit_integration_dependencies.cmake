# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include_guard(GLOBAL)

set(GRANIT_SDL3_LOCKED_VERSION "3.4.10")
set(GRANIT_IMGUI_LOCKED_VERSION "1.92.9")
# 锁定版本对应的 commit；下载用 commit 的不可变 tarball，避免 tag 被 force-push 后 SHA256 漂移。
set(GRANIT_SDL3_LOCKED_COMMIT "8e37db5e797b6167f3a00d697d816a684bd259c7")
set(GRANIT_IMGUI_LOCKED_COMMIT "01380c579715e62fb9a8d6ec0502c4ea83bfde6e")

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
  if(TARGET SDL3::SDL3)
    return()
  endif()
  include(FetchContent)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  if(GRANIT_ENABLE_WINDOW_SDL3)
    # Window 后端是已安装库的一部分，下载依赖时也要安装 SDL 的运行库与包配置，
    # 保证共享库可部署，并让静态 Consumer 能解析最终链接依赖。
    set(SDL_INSTALL ON CACHE BOOL "" FORCE)
  endif()
  FetchContent_Declare(
    granit_sdl3
    URL "https://github.com/libsdl-org/SDL/archive/${GRANIT_SDL3_LOCKED_COMMIT}.tar.gz"
    URL_HASH SHA256=85aa3f7b01e91d9a0b2e8079b065c594e579c43d53d5671c8f68b20074cc896e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  message(STATUS "正在获取 SDL3 ${GRANIT_SDL3_LOCKED_VERSION} 依赖")
  FetchContent_MakeAvailable(granit_sdl3)
  set(GRANIT_SDL3_FETCHED TRUE PARENT_SCOPE)
  set(granit_sdl3_SOURCE_DIR "${granit_sdl3_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

# 下载锁定版本 ImGui 并封装为统一目标。
function(granit_fetch_imgui)
  if(TARGET granit_imgui_dependency)
    return()
  endif()
  include(FetchContent)
  FetchContent_Declare(
    granit_imgui
    URL "https://github.com/ocornut/imgui/archive/${GRANIT_IMGUI_LOCKED_COMMIT}.tar.gz"
    URL_HASH SHA256=c7bc489afefa2461c40a84812118e6dff86e7eadfc4b7e2f851a8c94ade8910d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  message(STATUS "正在获取 ImGui ${GRANIT_IMGUI_LOCKED_VERSION} 依赖")
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

  add_library(
    granit_imgui_dependency_demo STATIC
    "${granit_imgui_SOURCE_DIR}/imgui_demo.cpp"
  )
  target_link_libraries(
    granit_imgui_dependency_demo
    PUBLIC granit_imgui_dependency
  )
  set_target_properties(
    granit_imgui_dependency_demo PROPERTIES POSITION_INDEPENDENT_CODE YES FOLDER "Third Party"
  )

  if(TARGET SDL3::SDL3)
    add_library(
      granit_imgui_dependency_backend_sdl3 STATIC
      "${granit_imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    )
    target_link_libraries(
      granit_imgui_dependency_backend_sdl3
      PUBLIC granit_imgui_dependency SDL3::SDL3
    )
    set_target_properties(
      granit_imgui_dependency_backend_sdl3 PROPERTIES POSITION_INDEPENDENT_CODE YES FOLDER "Third Party"
    )
  endif()

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
  set(GRANIT_SDL3_FETCHED "${GRANIT_SDL3_FETCHED}" PARENT_SCOPE)
  set(granit_sdl3_SOURCE_DIR "${granit_sdl3_SOURCE_DIR}" PARENT_SCOPE)

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
  set(GRANIT_IMGUI_FETCHED "${GRANIT_IMGUI_FETCHED}" PARENT_SCOPE)
  set(granit_imgui_SOURCE_DIR "${granit_imgui_SOURCE_DIR}" PARENT_SCOPE)

  if(TARGET granit_imgui_dependency)
    set(GRANIT_IMGUI_TARGET granit_imgui_dependency PARENT_SCOPE)
    if(TARGET granit_imgui_dependency_demo)
      set(GRANIT_IMGUI_DEMO_TARGET granit_imgui_dependency_demo PARENT_SCOPE)
    endif()
    if(TARGET granit_imgui_dependency_backend_sdl3)
      set(GRANIT_IMGUI_BACKEND_SDL3_TARGET granit_imgui_dependency_backend_sdl3 PARENT_SCOPE)
    endif()
  elseif(TARGET imgui::imgui)
    set(GRANIT_IMGUI_TARGET imgui::imgui PARENT_SCOPE)
    if(TARGET imgui::imgui_demo)
      set(GRANIT_IMGUI_DEMO_TARGET imgui::imgui_demo PARENT_SCOPE)
    endif()
    if(TARGET imgui::imgui_backend_sdl3)
      set(GRANIT_IMGUI_BACKEND_SDL3_TARGET imgui::imgui_backend_sdl3 PARENT_SCOPE)
    endif()
  elseif(TARGET ImGui::ImGui)
    set(GRANIT_IMGUI_TARGET ImGui::ImGui PARENT_SCOPE)
    if(TARGET ImGui::ImGui_Demo)
      set(GRANIT_IMGUI_DEMO_TARGET ImGui::ImGui_Demo PARENT_SCOPE)
    endif()
    if(TARGET ImGui::ImGui_Backend_SDL3)
      set(GRANIT_IMGUI_BACKEND_SDL3_TARGET ImGui::ImGui_Backend_SDL3 PARENT_SCOPE)
    endif()
  elseif(TARGET imgui)
    set(GRANIT_IMGUI_TARGET imgui PARENT_SCOPE)
    if(TARGET imgui_demo)
      set(GRANIT_IMGUI_DEMO_TARGET imgui_demo PARENT_SCOPE)
    endif()
    if(TARGET imgui_backend_sdl3)
      set(GRANIT_IMGUI_BACKEND_SDL3_TARGET imgui_backend_sdl3 PARENT_SCOPE)
    endif()
  else()
    message(FATAL_ERROR "已启用 ImGui Integration，但未找到可用的 ImGui 目标")
  endif()
endfunction()
