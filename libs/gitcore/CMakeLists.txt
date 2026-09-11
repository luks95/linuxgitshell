# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

add_library(
    linuxgitshell_gitcore STATIC
    include/linuxgitshell/gitcore/GitConfig.h
    include/linuxgitshell/gitcore/GitConfigReader.h
    include/linuxgitshell/gitcore/GitCommandSanitizer.h
    include/linuxgitshell/gitcore/GitProcessRunner.h
    include/linuxgitshell/gitcore/GitStatus.h
    include/linuxgitshell/gitcore/GitStatusReader.h
    include/linuxgitshell/gitcore/RepositoryDiscovery.h
    src/GitConfig.cpp
    src/GitConfigReader.cpp
    src/GitCommandSanitizer.cpp
    src/GitProcessRunner.cpp
    src/GitStatus.cpp
    src/GitStatusReader.cpp
    src/Logging.cpp
    src/ProjectInfo.cpp
    src/RepositoryDiscovery.cpp
)
add_library(LinuxGitShell::GitCore ALIAS linuxgitshell_gitcore)

target_include_directories(
    linuxgitshell_gitcore
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_compile_definitions(
    linuxgitshell_gitcore
    PRIVATE LINUXGITSHELL_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(
    linuxgitshell_gitcore
    PUBLIC
        Qt6::Core
        KF6::CoreAddons
)

linuxgitshell_set_compiler_warnings(linuxgitshell_gitcore)
