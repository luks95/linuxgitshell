# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

if(NOT CLANG_FORMAT_EXECUTABLE)
    message(FATAL_ERROR "clang-format was not found")
endif()

file(
    GLOB_RECURSE source_files
    LIST_DIRECTORIES false
    "${PROJECT_SOURCE_DIR}/gui/*.cpp"
    "${PROJECT_SOURCE_DIR}/gui/*.h"
    "${PROJECT_SOURCE_DIR}/integrations/*.cpp"
    "${PROJECT_SOURCE_DIR}/integrations/*.h"
    "${PROJECT_SOURCE_DIR}/libs/*.cpp"
    "${PROJECT_SOURCE_DIR}/libs/*.h"
    "${PROJECT_SOURCE_DIR}/tests/*.cpp"
    "${PROJECT_SOURCE_DIR}/tests/*.h"
)

execute_process(
    COMMAND "${CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror ${source_files}
    RESULT_VARIABLE format_result
)

if(NOT format_result EQUAL 0)
    message(FATAL_ERROR "C++ formatting check failed")
endif()
