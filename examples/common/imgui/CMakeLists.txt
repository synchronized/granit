# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 示例共享的 ImGui 与 Canvas 桥接。frame_canvas_data 只依赖 render_pipeline，无条件构建；
# ImGui DrawData 捕获与 Texture ID 注册依赖可选 ImGui 集成组件，仅在可用时追加。

add_library(granit_example_imgui_canvas STATIC frame_canvas_data.cpp frame_canvas_data.h)
target_compile_features(granit_example_imgui_canvas PUBLIC cxx_std_20)
target_include_directories(
  granit_example_imgui_canvas PUBLIC "${PROJECT_SOURCE_DIR}/examples/common"
)
target_link_libraries(granit_example_imgui_canvas PUBLIC granit::render_pipeline)
set_target_properties(granit_example_imgui_canvas PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_example_imgui_canvas)

if(TARGET granit::integration_imgui)
  add_library(
    granit_example_imgui STATIC
    imgui_frame_capture.cpp
    imgui_frame_capture.h
    imgui_texture_registry.cpp
    imgui_texture_registry.h
  )
  target_compile_features(granit_example_imgui PUBLIC cxx_std_20)
  target_include_directories(granit_example_imgui PUBLIC "${PROJECT_SOURCE_DIR}/examples/common")
  target_link_libraries(
    granit_example_imgui PUBLIC granit_example_imgui_canvas granit::integration_imgui
  )
  set_target_properties(granit_example_imgui PROPERTIES FOLDER "Examples")
  granit_target_compile_warnings(granit_example_imgui)

  if(GRANIT_TESTING_ENABLED)
    add_executable(
      granit_example_imgui_test imgui_frame_capture_test.cpp imgui_texture_registry_test.cpp
    )
    target_link_libraries(
      granit_example_imgui_test PRIVATE granit_example_imgui Catch2::Catch2WithMain
    )
    granit_target_compile_warnings(granit_example_imgui_test)
    add_test(NAME granit.example.imgui COMMAND granit_example_imgui_test)
    if(WIN32)
      set_tests_properties(
        granit.example.imgui
        PROPERTIES ENVIRONMENT_MODIFICATION
                   "PATH=path_list_prepend:$<TARGET_FILE_DIR:granit::integration_imgui>"
      )
    endif()
  endif()
endif()
