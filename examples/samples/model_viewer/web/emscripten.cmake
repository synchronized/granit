# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include(granit_web)

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
  PUBLIC granit::granit granit::window granit_sample_model_viewer_support granit_example_web
)
granit_target_webgpu(granit_web_model_viewer_platform)
granit_target_compile_warnings(granit_web_model_viewer_platform)

target_link_options(
  granit_web_model_viewer_platform INTERFACE
    "-sFETCH=1" "-sASYNCIFY=1"
)

# 面向使用者的浏览器模型查看器默认加载 Khronos Flight Helmet；`?model=<URL>` 可覆盖资产。
add_executable(granit_sample_model_viewer_web "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
target_compile_features(granit_sample_model_viewer_web PRIVATE cxx_std_20)
target_link_libraries(
  granit_sample_model_viewer_web PRIVATE granit::granit granit_web_model_viewer_platform
)

set_target_properties(
  granit_sample_model_viewer_web
  PROPERTIES OUTPUT_NAME granit_sample_model_viewer_web FOLDER "Examples/Samples"
)
granit_target_compile_warnings(granit_sample_model_viewer_web)

granit_target_web_page(
  granit_sample_model_viewer_web "${CMAKE_CURRENT_LIST_DIR}/model_viewer_shell.html"
)
