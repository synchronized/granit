# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

set(granit_xdg_shell_header "${granit_wayland_protocol_dir}/xdg-shell-client-protocol.h")
set(granit_xdg_shell_source "${granit_wayland_protocol_dir}/xdg-shell-protocol.c")
add_custom_command(
  OUTPUT "${granit_xdg_shell_header}" "${granit_xdg_shell_source}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${granit_wayland_protocol_dir}"
  COMMAND
    "${GRANIT_WAYLAND_SCANNER}" client-header "${GRANIT_XDG_SHELL_PROTOCOL}"
    "${granit_xdg_shell_header}"
  COMMAND
    "${GRANIT_WAYLAND_SCANNER}" private-code "${GRANIT_XDG_SHELL_PROTOCOL}"
    "${granit_xdg_shell_source}"
  DEPENDS "${GRANIT_XDG_SHELL_PROTOCOL}"
  VERBATIM
)
add_library(
  granit_xdg_shell_protocol OBJECT "${granit_xdg_shell_header}" "${granit_xdg_shell_source}"
)
set_target_properties(
  granit_xdg_shell_protocol
  PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE YES
)
target_include_directories(
  granit_xdg_shell_protocol SYSTEM PRIVATE "${GRANIT_WAYLAND_INCLUDE_DIR}"
                                         "${granit_wayland_protocol_dir}"
)
