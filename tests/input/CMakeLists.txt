# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_window_input_test input_contract_test.cpp)
target_link_libraries(granit_window_input_test PRIVATE granit::window Catch2::Catch2WithMain)
granit_target_compile_warnings(granit_window_input_test)
granit_target_output_directories(granit_window_input_test)
add_test(NAME granit.window_input COMMAND granit_window_input_test)
set_property(TEST granit.window_input APPEND PROPERTY LABELS unit platform)

if(WIN32)
  add_executable(granit_win32_input_integration_test win32_input_integration_test.cpp)
  target_link_libraries(
    granit_win32_input_integration_test PRIVATE granit::window Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_win32_input_integration_test)
  granit_target_output_directories(granit_win32_input_integration_test)
  add_test(NAME granit.window_input.win32 COMMAND granit_win32_input_integration_test)
  set_property(TEST granit.window_input.win32 APPEND PROPERTY LABELS integration platform)
endif()

add_executable(
  granit_emscripten_input_adapter_test
  emscripten_input_adapter_test.cpp
  "${PROJECT_SOURCE_DIR}/src/window/platform/emscripten/input.cpp"
)
target_include_directories(
  granit_emscripten_input_adapter_test
  PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(granit_emscripten_input_adapter_test PRIVATE Catch2::Catch2WithMain)
granit_target_compile_warnings(granit_emscripten_input_adapter_test)
granit_target_output_directories(granit_emscripten_input_adapter_test)
add_test(NAME granit.window_input.emscripten_adapter COMMAND granit_emscripten_input_adapter_test)
set_property(TEST granit.window_input.emscripten_adapter APPEND PROPERTY LABELS unit platform)

if(GRANIT_HAS_XCB)
  add_executable(
    granit_xcb_input_adapter_test
    xcb_input_adapter_test.cpp
    "${PROJECT_SOURCE_DIR}/src/window/platform/xcb/input.cpp"
  )
  target_include_directories(
    granit_xcb_input_adapter_test
    PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
  )
  target_link_libraries(granit_xcb_input_adapter_test PRIVATE Catch2::Catch2WithMain)
  granit_target_compile_warnings(granit_xcb_input_adapter_test)
  granit_target_output_directories(granit_xcb_input_adapter_test)
  add_test(NAME granit.window_input.xcb_adapter COMMAND granit_xcb_input_adapter_test)
  set_property(TEST granit.window_input.xcb_adapter APPEND PROPERTY LABELS unit platform)
endif()

if(GRANIT_HAS_WAYLAND_INPUT)
  add_executable(
    granit_wayland_input_adapter_test
    wayland_input_adapter_test.cpp
    "${PROJECT_SOURCE_DIR}/src/window/platform/wayland/input.cpp"
  )
  target_include_directories(
    granit_wayland_input_adapter_test
    PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src"
  )
  target_include_directories(
    granit_wayland_input_adapter_test SYSTEM PRIVATE "${GRANIT_XKBCOMMON_INCLUDE_DIR}"
  )
  target_link_libraries(
    granit_wayland_input_adapter_test PRIVATE Catch2::Catch2WithMain "${GRANIT_XKBCOMMON_LIBRARY}"
  )
  granit_target_compile_warnings(granit_wayland_input_adapter_test)
  granit_target_output_directories(granit_wayland_input_adapter_test)
  add_test(NAME granit.window_input.wayland_adapter COMMAND granit_wayland_input_adapter_test)
  set_property(TEST granit.window_input.wayland_adapter APPEND PROPERTY LABELS unit platform)
endif()
