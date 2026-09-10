# Licensing and SPDX policy

LinuxGitShell source code and project-authored resources are licensed under the MIT License. New C++,
CMake, scripts, and translation files must begin with:

```text
SPDX-FileCopyrightText: YEAR LinuxGitShell contributors
SPDX-License-Identifier: MIT
```

Use the comment syntax appropriate to the file. Documentation is covered by the repository `LICENSE`;
SPDX headers are optional where they would harm readability. Third-party assets must retain their
original copyright, license, source, and attribution instead of receiving the project header.

## Dependency review

The bootstrap dynamically uses Qt 6 (`GPL-3.0-only`, `LGPL-3.0-only`, commercial alternatives and
the Qt GPL exception in the installed package), KDE CoreAddons/KI18n (`LGPL-2.0-only` or
`LGPL-3.0-only`), and invokes the system Git executable (`GPL-2.0-only`) as a separate process. CMake,
Ninja, GCC, Clang, and Gettext are build tools rather than incorporated source.

The installed Arch/Manjaro package metadata was reviewed on 2026-09-09. These licenses permit the
planned MIT-licensed project structure, provided that LinuxGitShell does not copy dependency source
or relicense third-party assets. Every new linked dependency, bundled component, icon, or other asset
must receive a fresh compatibility review before merge. This document is a project engineering record,
not legal advice.
