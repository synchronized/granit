# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(granit_model_viewer_desktop_shell STATIC desktop_options.cpp desktop_options.h)
target_compile_features(granit_model_viewer_desktop_shell PUBLIC cxx_std_20)
target_include_directories(
  granit_model_viewer_desktop_shell PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(granit_model_viewer_desktop_shell PUBLIC granit::granit)
set_target_properties(granit_model_viewer_desktop_shell PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_model_viewer_desktop_shell)

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_executable(granit_model_viewer_desktop_shell_test desktop_options_test.cpp)
  target_link_libraries(
    granit_model_viewer_desktop_shell_test
    PRIVATE granit_model_viewer_desktop_shell Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_model_viewer_desktop_shell_test)
  add_test(
    NAME granit.example.model_viewer_desktop_shell
    COMMAND granit_model_viewer_desktop_shell_test
  )
endif()
