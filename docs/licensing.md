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

## Icons

The five overlay emblems under `icons/hicolor/scalable/emblems/` (`emblem-linuxgitshell-clean`,
`-modified`, `-added`, `-conflict`, `-untracked`) are original SVG artwork written by the
LinuxGitShell contributors for this project and licensed under the MIT License. They are simple
geometric shapes drawn from scratch; no third-party artwork, icon-theme file, font, or trace was
used, so there is no external source or attribution to retain. Each file carries the SPDX header.
Shape and glyph, not color alone, distinguish the states. The deleted and ignored glyphs are not
authored yet. Any future asset taken from another source must be recorded here with its source,
license, and attribution before merge.

## Dependency review

LinuxGitShell dynamically uses Qt 6 (`GPL-3.0-only`, `LGPL-3.0-only`, commercial alternatives and
the Qt GPL exception in the installed package), KDE CoreAddons/KI18n/KIO (LGPL-family licenses in
the installed packages), and invokes the system Git executable (`GPL-2.0-only`) as a separate
process. Extra CMake Modules, CMake, Ninja, GCC, Clang, and Gettext are build tools rather than
incorporated source.

The installed Arch/Manjaro package metadata was reviewed initially on 2026-09-09 and extended for
KIO and Extra CMake Modules on 2026-09-13. These licenses permit the
planned MIT-licensed project structure, provided that LinuxGitShell does not copy dependency source
or relicense third-party assets. Every new linked dependency, bundled component, icon, or other asset
must receive a fresh compatibility review before merge. This document is a project engineering record,
not legal advice.
