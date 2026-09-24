# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include(granit_web)

add_library(
  granit_web_model_viewer_platform STATIC
  "${CMAKE_CURRENT_LIST_DIR}/application.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/application.h"
)
function(granit_configure_web_model_viewer_platform target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_include_directories(
    ${target}
    PUBLIC "${PROJECT_SOURCE_DIR}/examples/samples"
           "${PROJECT_SOURCE_DIR}/examples/common"
  )
  target_link_libraries(
    ${target}
    PUBLIC granit::granit granit_sample_model_viewer_application
  )
  granit_target_webgpu(${target})
  granit_target_compile_warnings(${target})
  target_link_options(${target} INTERFACE "-sASYNCIFY=1")
endfunction()

granit_configure_web_model_viewer_platform(granit_web_model_viewer_platform)

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_library(
    granit_web_model_viewer_test_platform STATIC
    "${CMAKE_CURRENT_LIST_DIR}/application.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/application.h"
    "${CMAKE_CURRENT_LIST_DIR}/browser_test_api.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/browser_test_control.h"
    "${CMAKE_CURRENT_LIST_DIR}/browser_test_hooks.h"
    "${CMAKE_CURRENT_LIST_DIR}/pipeline_validation.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/pipeline_validation.h"
  )
  target_compile_definitions(
    granit_web_model_viewer_test_platform PRIVATE GRANIT_MODEL_VIEWER_BROWSER_TESTS=1
  )
  granit_configure_web_model_viewer_platform(granit_web_model_viewer_test_platform)
endif()

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

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  # 自动化目标额外编入浏览器控制导出和 C API Pipeline 生命周期探针。
  add_executable(granit_sample_model_viewer_web_test "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
  target_compile_features(granit_sample_model_viewer_web_test PRIVATE cxx_std_20)
  target_link_libraries(
    granit_sample_model_viewer_web_test
    PRIVATE granit::granit granit_web_model_viewer_test_platform
  )
  set_target_properties(
    granit_sample_model_viewer_web_test
    PROPERTIES OUTPUT_NAME granit_sample_model_viewer_web_test FOLDER "Tests/Browser"
  )
  granit_target_compile_warnings(granit_sample_model_viewer_web_test)
  granit_target_web_page(
    granit_sample_model_viewer_web_test "${CMAKE_CURRENT_LIST_DIR}/model_viewer_shell.html"
  )
endif()
