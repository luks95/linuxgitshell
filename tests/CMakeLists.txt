# SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
# SPDX-License-Identifier: MIT

add_executable(ProjectInfoTest ProjectInfoTest.cpp)

target_link_libraries(
    ProjectInfoTest
    PRIVATE
        LinuxGitShell::GitCore
        Qt6::Test
)

linuxgitshell_set_compiler_warnings(ProjectInfoTest)

add_test(NAME gitcore.project_info COMMAND ProjectInfoTest)

add_executable(TranslationTest TranslationTest.cpp)

target_compile_definitions(
    TranslationTest
    PRIVATE TRANSLATION_LOCALE_DIR="${CMAKE_BINARY_DIR}/po"
)

target_link_libraries(
    TranslationTest
    PRIVATE
        Qt6::Test
        KF6::I18n
)

add_dependencies(TranslationTest linuxgitshell_translations)
linuxgitshell_set_compiler_warnings(TranslationTest)

add_test(NAME i18n.spanish_catalog COMMAND TranslationTest)
set_tests_properties(
    i18n.spanish_catalog
    PROPERTIES ENVIRONMENT "LC_ALL=es_PY.UTF-8;LANGUAGE=es"
)
