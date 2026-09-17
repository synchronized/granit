# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 示例共享的浏览器资源加载基础设施。fetch.cpp 依赖 Emscripten，由浏览器示例运行层编译，
# 不进入本纯 C++ 库，使桌面侧也能构建并测试资源请求与包管理。

add_library(
  granit_example_web STATIC
  asset_request.cpp
  asset_request.h
  resource_bundle.cpp
  resource_bundle.h
  resource_fetch_batch.cpp
  resource_fetch_batch.h
)
target_compile_features(granit_example_web PUBLIC cxx_std_20)
target_include_directories(granit_example_web PUBLIC "${PROJECT_SOURCE_DIR}/examples/common")
target_link_libraries(granit_example_web PUBLIC granit_example_gltf_support)
set_target_properties(granit_example_web PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_example_web)

if(NOT EMSCRIPTEN AND GRANIT_TESTING_ENABLED)
  add_executable(
    granit_example_web_test
    asset_request_test.cpp
    resource_bundle_test.cpp
    resource_fetch_batch_test.cpp
  )
  target_link_libraries(granit_example_web_test PRIVATE granit_example_web Catch2::Catch2WithMain)
  granit_target_compile_warnings(granit_example_web_test)
  add_test(NAME granit.example.web COMMAND granit_example_web_test)
endif()
