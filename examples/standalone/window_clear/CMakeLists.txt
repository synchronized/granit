# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

cmake_minimum_required(VERSION 3.23)
project(granit_window_clear LANGUAGES CXX)

include(CTest)

find_package(granit CONFIG REQUIRED COMPONENTS Window)

add_executable(granit_window_clear main.cpp)
target_compile_features(granit_window_clear PRIVATE cxx_std_20)
target_link_libraries(granit_window_clear PRIVATE granit::window)

if(BUILD_TESTING)
  add_test(NAME granit.standalone.window_clear COMMAND granit_window_clear --smoke-test)
  set_tests_properties(
    granit.standalone.window_clear
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30
  )
  if(WIN32)
    set_tests_properties(
      granit.standalone.window_clear
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::window>"
    )
  elseif(UNIX)
    set_tests_properties(
      granit.standalone.window_clear
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::window>"
    )
  endif()
endif()
