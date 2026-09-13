# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

# 只生成项目级包配置；各组件的目标与安装声明由对应源码目录维护。
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/granitConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/granitConfig.cmake"
  INSTALL_DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/granitConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMinorVersion
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/granitConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/granitConfigVersion.cmake"
  DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
)

install(
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GranitShaderToolchain.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GranitShaderToolchainLock.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/granit_shader_toolchain.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/download_shader_toolchain.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_shader_toolchain_manifest.cmake"
  DESTINATION "${GRANIT_INSTALL_CMAKEDIR}"
  COMPONENT AssetTools
)

install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DOCDIR}")
