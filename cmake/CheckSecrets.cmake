# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

file(
    GLOB_RECURSE candidate_files
    LIST_DIRECTORIES false
    "${PROJECT_SOURCE_DIR}/*"
)

foreach(candidate IN LISTS candidate_files)
    if(candidate MATCHES "/(build[^/]*|install[^/]*|\\.git)/")
        continue()
    endif()

    file(SIZE "${candidate}" candidate_size)
    if(candidate_size GREATER 1048576)
        continue()
    endif()

    file(READ "${candidate}" candidate_content)
    if(candidate_content MATCHES "BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY")
        message(FATAL_ERROR "Potential private key found in ${candidate}")
    endif()
    if(candidate_content MATCHES "https?://[^/@ \\n]+:[^/@ \\n]+@")
        message(FATAL_ERROR "Potential credential-bearing URL found in ${candidate}")
    endif()
endforeach()
