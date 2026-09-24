# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.25)

project(
    LinuxGitShell
    VERSION 0.1.0
    DESCRIPTION "Native Git desktop integration for Linux"
    LANGUAGES CXX
)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(PREPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LinuxGitShellCompilerWarnings)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus Widgets)
find_package(KF6CoreAddons 6.0 REQUIRED)
find_package(KF6I18n 6.0 REQUIRED)
find_package(KF6KIO 6.0 REQUIRED)
find_package(Git REQUIRED)

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)

add_subdirectory(libs/gitcore)
add_subdirectory(libs/repositorycontext)
add_subdirectory(daemon)
add_subdirectory(gui/app)
add_subdirectory(integrations/dolphin/contextmenu)
add_subdirectory(po)

if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(
        format-check
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DCLANG_FORMAT_EXECUTABLE=${CLANG_FORMAT_EXECUTABLE}"
            -P "${CMAKE_SOURCE_DIR}/cmake/CheckFormatting.cmake"
        COMMENT "Checking C++ formatting"
        VERBATIM
    )
endif()

add_custom_target(
    check-secrets
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/cmake/CheckSecrets.cmake"
    COMMENT "Checking for common secret patterns"
    VERBATIM
)

if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
