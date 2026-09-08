# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if(EMSCRIPTEN)
  if(GRANIT_BUILD_EXAMPLES AND GRANIT_BUILD_WEB_IMGUI_EXAMPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/web.cmake")
  endif()
  return()
endif()

if(NOT GRANIT_BUILD_INTEGRATION_SDL3 OR NOT GRANIT_BUILD_INTEGRATION_IMGUI OR
   NOT GRANIT_IMGUI_FETCHED)
  return()
endif()

add_executable(
  granit_sdl3_imgui_example
  desktop_main.cpp
  resources.cpp
  "${granit_imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${granit_imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
)
target_include_directories(
  granit_sdl3_imgui_example
  PRIVATE "${PROJECT_SOURCE_DIR}/examples" "${PROJECT_SOURCE_DIR}/examples/common"
)
target_include_directories(
  granit_sdl3_imgui_example SYSTEM PRIVATE "${granit_imgui_SOURCE_DIR}"
)
target_link_libraries(
  granit_sdl3_imgui_example PRIVATE granit::integration_sdl3 granit::integration_imgui
)
if(WIN32)
  add_custom_command(
    TARGET granit_sdl3_imgui_example
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${GRANIT_SDL3_TARGET}>"
      "$<TARGET_FILE_DIR:granit_sdl3_imgui_example>"
    VERBATIM
  )
endif()
granit_target_compile_warnings(granit_sdl3_imgui_example)
granit_target_output_directories(granit_sdl3_imgui_example)
if(WIN32 AND GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_test(NAME granit.example.sdl3_imgui COMMAND granit_sdl3_imgui_example --smoke-test)
  set_tests_properties(granit.example.sdl3_imgui PROPERTIES TIMEOUT 30)
endif()
