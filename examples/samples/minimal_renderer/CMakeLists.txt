# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_executable(granit_minimal_renderer_example main.cpp)
target_link_libraries(granit_minimal_renderer_example PRIVATE granit::granit)
target_compile_features(granit_minimal_renderer_example PRIVATE cxx_std_20)
granit_target_compile_warnings(granit_minimal_renderer_example)
granit_target_output_directories(granit_minimal_renderer_example)

if(GRANIT_BUILD_TESTING AND BUILD_TESTING AND NOT EMSCRIPTEN)
  add_test(NAME granit.example.minimal_renderer COMMAND granit_minimal_renderer_example)
  set_tests_properties(
    granit.example.minimal_renderer
    PROPERTIES
      TIMEOUT 30
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
  )
  if(APPLE)
    set_tests_properties(
      granit.example.minimal_renderer
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
    )
  elseif(UNIX)
    set_tests_properties(
      granit.example.minimal_renderer
      PROPERTIES ENVIRONMENT_MODIFICATION
                 "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::granit>"
    )
  endif()
endif()
