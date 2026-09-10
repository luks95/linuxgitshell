# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

find_package(Gettext REQUIRED)

set(spanish_po "${CMAKE_CURRENT_SOURCE_DIR}/es/linuxgitshell.po")
set(spanish_mo "${CMAKE_CURRENT_BINARY_DIR}/es/LC_MESSAGES/linuxgitshell.mo")

add_custom_command(
    OUTPUT "${spanish_mo}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/es/LC_MESSAGES"
    COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}" --check "--output-file=${spanish_mo}" "${spanish_po}"
    DEPENDS "${spanish_po}"
    COMMENT "Compiling Spanish translations"
    VERBATIM
)

add_custom_target(linuxgitshell_translations ALL DEPENDS "${spanish_mo}")

install(
    FILES "${spanish_mo}"
    DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/es/LC_MESSAGES"
)
