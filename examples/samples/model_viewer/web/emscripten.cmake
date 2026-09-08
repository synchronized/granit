# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

add_library(
  granit_web_model_viewer_platform STATIC
  "${CMAKE_CURRENT_LIST_DIR}/application.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/application.h"
  "${PROJECT_SOURCE_DIR}/examples/common/web/fetch.cpp"
  "${PROJECT_SOURCE_DIR}/examples/common/web/fetch.h"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/web/web_input.cpp"
  "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/web/web_input.h"
)
target_compile_features(granit_web_model_viewer_platform PUBLIC cxx_std_20)
target_include_directories(
  granit_web_model_viewer_platform
  PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples"
         "${PROJECT_SOURCE_DIR}/examples/common"
)
target_link_libraries(
  granit_web_model_viewer_platform
  PUBLIC granit::granit granit_example_model_viewer_support granit_example_web
)
target_compile_options(
  granit_web_model_viewer_platform PRIVATE "--use-port=emdawnwebgpu:cpp_bindings=false"
)
granit_target_compile_warnings(granit_web_model_viewer_platform)

target_link_options(
  granit_web_model_viewer_platform INTERFACE
    "-sALLOW_MEMORY_GROWTH=1" "-sNO_EXIT_RUNTIME=1" "-sFETCH=1" "-sASYNCIFY=1"
    "$<$<CONFIG:Debug>:-sASSERTIONS=1>"
)

if(NOT GRANIT_BUILD_EXAMPLES OR NOT GRANIT_BUILD_MODEL_VIEWER_EXAMPLE)
  return()
endif()

# 面向使用者的浏览器模型查看器默认加载 Khronos Flight Helmet；`?model=<URL>` 可覆盖资产。
add_executable(granit_model_viewer_web "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
target_compile_features(granit_model_viewer_web PRIVATE cxx_std_20)
target_link_libraries(
  granit_model_viewer_web PRIVATE granit::granit granit_web_model_viewer_platform
)
target_link_options(
  granit_model_viewer_web
  PRIVATE
    "--use-port=emdawnwebgpu:cpp_bindings=false"
    "--shell-file=${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/web/model_viewer_shell.html"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_EXIT_RUNTIME=1"
    "-sFETCH=1"
    "-sASYNCIFY=1"
    "$<$<CONFIG:Debug>:-sASSERTIONS=1>"
)
set_target_properties(
  granit_model_viewer_web
  PROPERTIES OUTPUT_NAME granit_model_viewer_web SUFFIX ".html" FOLDER "Examples"
             LINK_DEPENDS "${PROJECT_SOURCE_DIR}/examples/samples/model_viewer/web/model_viewer_shell.html"
)
granit_target_compile_warnings(granit_model_viewer_web)

set_target_properties(granit_model_viewer_web PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/web")
