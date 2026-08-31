# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

include(FetchContent)

set(cgltf_target "")
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

set(stb_image_target "")
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

if((NOT cgltf_target OR NOT stb_image_target) AND GRANIT_FETCH_EXAMPLE_GLTF_DEPENDENCIES)
  if(NOT cgltf_target)
    FetchContent_Declare(
      granit_example_cgltf
      URL
        https://github.com/jkuhlmann/cgltf/archive/360db1a95480fe102ae9c69b27c5d101167ff5ba.tar.gz
      URL_HASH SHA256=445d135cf793232ae6a585ca1404e4ff28d4f4dbca070689034fe780370ac84a
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(granit_example_cgltf)
    add_library(granit_example_cgltf_headers INTERFACE)
    target_include_directories(
      granit_example_cgltf_headers INTERFACE "${granit_example_cgltf_SOURCE_DIR}"
    )
    set(cgltf_target granit_example_cgltf_headers)
  endif()
  if(NOT stb_image_target)
    FetchContent_Declare(
      granit_example_stb
      URL
        https://github.com/nothings/stb/archive/013ac3beddff3dbffafd5177e7972067cd2b5083.tar.gz
      URL_HASH SHA256=b01aa93e1a968aed55f43e072c98ee401d2f20e897aabdb1a166c7166886ed11
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(granit_example_stb)
    add_library(granit_example_stb_image_headers INTERFACE)
    target_include_directories(
      granit_example_stb_image_headers INTERFACE "${granit_example_stb_SOURCE_DIR}"
    )
    set(stb_image_target granit_example_stb_image_headers)
  endif()
endif()

if(NOT cgltf_target OR NOT stb_image_target)
  message(
    FATAL_ERROR
      "模型查看器需要 cgltf 1.15 与 stb_image 2.30；请提供父项目 Target、find_package 包，"
      "或启用 GRANIT_FETCH_EXAMPLE_GLTF_DEPENDENCIES"
  )
endif()

add_library(granit_example_gltf_dependencies INTERFACE)
target_link_libraries(
  granit_example_gltf_dependencies INTERFACE "${cgltf_target}" "${stb_image_target}"
)
set_target_properties(granit_example_gltf_dependencies PROPERTIES FOLDER "Examples/Third Party")

add_library(granit_example_gltf_support STATIC loader.cpp resource_uri.cpp)
if(MSVC OR WIN32)
  target_compile_definitions(granit_example_gltf_support PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_compile_features(granit_example_gltf_support PUBLIC cxx_std_20)
target_include_directories(granit_example_gltf_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(
  granit_example_gltf_support
  PRIVATE granit_example_gltf_dependencies
  PUBLIC granit::math
)
set_target_properties(granit_example_gltf_support PROPERTIES FOLDER "Examples")
granit_target_compile_warnings(granit_example_gltf_support)

if(GRANIT_BUILD_TESTING AND BUILD_TESTING)
  add_executable(granit_example_gltf_support_test resource_uri_test.cpp)
  target_link_libraries(
    granit_example_gltf_support_test
    PRIVATE granit_example_gltf_support Catch2::Catch2WithMain
  )
  granit_target_compile_warnings(granit_example_gltf_support_test)
  add_test(NAME granit.example.gltf_support COMMAND granit_example_gltf_support_test)
endif()
