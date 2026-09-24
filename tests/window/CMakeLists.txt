# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 公共契约在每个平台使用同一份源码；Emscripten 负责构建，浏览器 smoke 负责运行。
add_executable(granit_window_contract_test contract_test.cpp)
target_link_libraries(granit_window_contract_test PRIVATE granit::window Catch2::Catch2WithMain)
granit_target_compile_warnings(granit_window_contract_test)
granit_target_output_directories(granit_window_contract_test)

# 原始事件解码器不创建系统窗口，可以在任意主机上覆盖所有无外部依赖的格式。
add_executable(
  granit_window_adapter_test
  adapters/emscripten_input_test.cpp
  adapters/xcb_input_test.cpp
  "${PROJECT_SOURCE_DIR}/src/window/platform/emscripten/input.cpp"
  "${PROJECT_SOURCE_DIR}/src/window/platform/xcb/input.cpp"
)
target_include_directories(
  granit_window_adapter_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(granit_window_adapter_test PRIVATE Catch2::Catch2WithMain)
if(GRANIT_HAS_WAYLAND_INPUT)
  target_sources(
    granit_window_adapter_test
    PRIVATE adapters/wayland_input_test.cpp
            "${PROJECT_SOURCE_DIR}/src/window/platform/wayland/input.cpp"
  )
  target_include_directories(
    granit_window_adapter_test SYSTEM PRIVATE "${GRANIT_XKBCOMMON_INCLUDE_DIR}"
  )
  target_link_libraries(granit_window_adapter_test PRIVATE "${GRANIT_XKBCOMMON_LIBRARY}")
endif()
granit_target_compile_warnings(granit_window_adapter_test)
granit_target_output_directories(granit_window_adapter_test)

if(NOT GRANIT_IS_EMSCRIPTEN)
  add_test(NAME granit.window.contract COMMAND granit_window_contract_test)
  add_test(NAME granit.window.adapters COMMAND granit_window_adapter_test)
  set_tests_properties(
    granit.window.contract granit.window.adapters PROPERTIES LABELS "unit;platform"
  )
endif()

if(GRANIT_HAS_NATIVE_WINDOW)
  add_executable(
    granit_window_platform_test
    platform/window_test.cpp
    $<$<PLATFORM_ID:Windows>:platform/win32_input_test.cpp>
  )
  target_link_libraries(granit_window_platform_test PRIVATE granit::window Catch2::Catch2WithMain)
  if(GRANIT_HAS_XCB)
    target_compile_definitions(granit_window_platform_test PRIVATE GRANIT_TEST_HAS_XCB)
    target_include_directories(
      granit_window_platform_test SYSTEM PRIVATE "${GRANIT_XCB_INCLUDE_DIR}"
    )
    target_link_libraries(granit_window_platform_test PRIVATE "${GRANIT_XCB_LIBRARY}")
  endif()
  if(GRANIT_HAS_WAYLAND_SHELL)
    target_compile_definitions(granit_window_platform_test PRIVATE GRANIT_TEST_HAS_WAYLAND)
  endif()
  if(GRANIT_ENABLE_WINDOW_SDL3)
    target_sources(granit_window_platform_test PRIVATE platform/sdl3_window_test.cpp)
    target_link_libraries(granit_window_platform_test PRIVATE "${GRANIT_SDL3_TARGET}")
  endif()
  granit_target_compile_warnings(granit_window_platform_test)
  granit_target_output_directories(granit_window_platform_test)
  add_test(NAME granit.window.platform COMMAND granit_window_platform_test)
  set_property(TEST granit.window.platform APPEND PROPERTY LABELS integration platform)

  add_executable(granit_window_renderer_test renderer_test.cpp)
  target_link_libraries(
    granit_window_renderer_test PRIVATE granit::window granit::granit Catch2::Catch2WithMain
  )
  if(GRANIT_HAS_XCB)
    target_compile_definitions(granit_window_renderer_test PRIVATE GRANIT_TEST_HAS_XCB)
  endif()
  if(GRANIT_HAS_WAYLAND_SHELL)
    target_compile_definitions(granit_window_renderer_test PRIVATE GRANIT_TEST_HAS_WAYLAND)
  endif()
  granit_target_compile_warnings(granit_window_renderer_test)
  granit_target_output_directories(granit_window_renderer_test)
  add_test(NAME granit.window.renderer COMMAND granit_window_renderer_test)
  set_property(TEST granit.window.renderer APPEND PROPERTY LABELS gpu integration platform)
endif()
