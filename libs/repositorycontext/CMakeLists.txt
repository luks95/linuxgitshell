# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

# Snapshot model and bounded cache shared by the Dolphin plugin client and the context service.
# It must stay free of Git execution, filesystem discovery, and IPC.
add_library(
    linuxgitshell_repositorycontext STATIC
    include/linuxgitshell/repositorycontext/RepositoryContextCache.h
    include/linuxgitshell/repositorycontext/RepositoryContextSnapshot.h
    src/RepositoryContextCache.cpp
    src/RepositoryContextSnapshot.cpp
)
add_library(LinuxGitShell::RepositoryContext ALIAS linuxgitshell_repositorycontext)

set_target_properties(
    linuxgitshell_repositorycontext
    PROPERTIES POSITION_INDEPENDENT_CODE ON
)

target_include_directories(
    linuxgitshell_repositorycontext
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(linuxgitshell_repositorycontext PUBLIC Qt6::Core)

linuxgitshell_set_compiler_warnings(linuxgitshell_repositorycontext)
