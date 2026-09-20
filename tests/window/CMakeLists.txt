# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_window_test window_api_test.cpp)
target_link_libraries(granit_window_test PRIVATE granit::window Catch2::Catch2WithMain)
if(GRANIT_HAS_XCB)
  target_compile_definitions(granit_window_test PRIVATE GRANIT_TEST_HAS_XCB)
  target_include_directories(granit_window_test SYSTEM PRIVATE "${GRANIT_XCB_INCLUDE_DIR}")
  target_link_libraries(granit_window_test PRIVATE "${GRANIT_XCB_LIBRARY}")
endif()
if(GRANIT_HAS_WAYLAND_SHELL)
  target_compile_definitions(granit_window_test PRIVATE GRANIT_TEST_HAS_WAYLAND)
endif()
granit_target_compile_warnings(granit_window_test)
granit_target_output_directories(granit_window_test)
add_test(NAME granit.window COMMAND granit_window_test)
set_property(TEST granit.window APPEND PROPERTY LABELS unit platform)

add_executable(granit_window_renderer_test window_renderer_test.cpp)
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
add_test(NAME granit.window_renderer COMMAND granit_window_renderer_test)
set_property(TEST granit.window_renderer APPEND PROPERTY LABELS gpu integration platform)
