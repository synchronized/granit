# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

granit_dependency_policy(CGLTF cgltf_policy)
granit_dependency_policy(STB stb_policy)

# cgltf：download 策略跳过系统查找；system/auto 先尝试父项目目标与 find_package。
set(cgltf_target "")
if(NOT cgltf_policy STREQUAL "download")
  if(TARGET cgltf::cgltf)
    set(cgltf_target cgltf::cgltf)
  elseif(TARGET cgltf)
    set(cgltf_target cgltf)
  else()
    find_package(cgltf 1.15 CONFIG QUIET)
    if(TARGET cgltf::cgltf)
      set(cgltf_target cgltf::cgltf)
    elseif(TARGET cgltf)
      set(cgltf_target cgltf)
    endif()
  endif()
endif()
if(NOT cgltf_target AND (cgltf_policy STREQUAL "auto" OR cgltf_policy STREQUAL "download"))
  granit_prepare_source_cache(
    "cgltf" "${GRANIT_CGLTF_LOCKED_COMMIT}" "${GRANIT_CGLTF_LOCKED_URL}"
    "${GRANIT_CGLTF_LOCKED_SHA256}" "${GRANIT_CGLTF_LOCKED_MARKER}" cgltf_source_dir)
  add_library(granit_example_cgltf_headers INTERFACE)
  target_include_directories(
    granit_example_cgltf_headers SYSTEM INTERFACE "${cgltf_source_dir}"
  )
  set(cgltf_target granit_example_cgltf_headers)
endif()

# stb_image：download 策略跳过系统查找；system/auto 先尝试父项目目标与 find_package。
set(stb_image_target "")
if(NOT stb_policy STREQUAL "download")
  if(TARGET stb::image)
    set(stb_image_target stb::image)
  elseif(TARGET Stb::Stb)
    set(stb_image_target Stb::Stb)
  else()
    find_package(Stb QUIET)
    if(TARGET Stb::Stb)
      set(stb_image_target Stb::Stb)
    endif()
  endif()
endif()
if(NOT stb_image_target AND (stb_policy STREQUAL "auto" OR stb_policy STREQUAL "download"))
  granit_prepare_source_cache(
    "stb" "${GRANIT_STB_LOCKED_COMMIT}" "${GRANIT_STB_LOCKED_URL}"
    "${GRANIT_STB_LOCKED_SHA256}" "${GRANIT_STB_LOCKED_MARKER}" stb_source_dir)
  add_library(granit_example_stb_image_headers INTERFACE)
  target_include_directories(
    granit_example_stb_image_headers SYSTEM INTERFACE "${stb_source_dir}"
  )
  set(stb_image_target granit_example_stb_image_headers)
endif()

if(NOT cgltf_target OR NOT stb_image_target)
  message(
    FATAL_ERROR
      "模型查看器需要 cgltf 1.15 与 stb_image 2.30；请提供父项目 Target、find_package 包，"
      "或将 GRANIT_DEPENDENCY_POLICY 设为 auto/download"
  )
endif()

add_library(granit_example_gltf_dependencies INTERFACE)
target_link_libraries(
  granit_example_gltf_dependencies INTERFACE "${cgltf_target}" "${stb_image_target}"
)
set_target_properties(granit_example_gltf_dependencies PROPERTIES FOLDER "Examples/Third Party")

add_library(
  granit_example_gltf_support STATIC
  asset_loaders.cpp
  asset_loaders.h
  cgltf_implementation.cpp
  document_manifest.cpp
  document_manifest.h
  image_decoder.cpp
  importer.cpp
  importer.h
)
if(MSVC OR WIN32)
  target_compile_definitions(granit_example_gltf_support PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_compile_features(granit_example_gltf_support PUBLIC cxx_std_20)
target_include_directories(granit_example_gltf_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(
  granit_example_gltf_support
  PRIVATE granit_example_gltf_dependencies
  PUBLIC granit_example_assets granit::granit
)
set_target_properties(granit_example_gltf_support PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_example_gltf_support)

if(GRANIT_HAS_NATIVE_WINDOW AND GRANIT_TESTING_ENABLED)
  add_executable(granit_example_gltf_support_test gltf_test.cpp)
  target_link_libraries(
    granit_example_gltf_support_test
    PRIVATE granit_example_gltf_support Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_example_gltf_support_test)
  add_test(NAME granit.example.gltf_support COMMAND granit_example_gltf_support_test)
endif()
