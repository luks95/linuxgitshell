# Development Environment

This snapshot records the environment first inspected on 2026-09-09 and updated for the Phase 2
Dolphin work on 2026-09-13. It is evidence of a working development setup, not the final support
matrix.

## Platform

| Component | Detected version |
| --- | --- |
| Distribution | Manjaro Linux, rolling |
| KDE Plasma (`plasma-desktop`) | 6.7.4-1 |
| Plasma Workspace | 6.7.4-3 |
| Dolphin | 26.08.0-5 |
| Qt (`qt6-base`) | 6.11.2-3 |
| KDE Frameworks | 6.29.0 |
| LinuxGitShell | `v0.1.0` released; Phase 2 development on `main` |

## Toolchain and packages

| Package/tool | Detected version | Current role |
| --- | --- | --- |
| GCC | 16.2.1 | C++20 compiler |
| CMake | 4.4.3 | Configure and generate |
| Ninja | 1.13.2 | Build executor |
| Git | 2.55.0 | Required external Git implementation |
| Gettext | 1.0 | Spanish translation catalog compilation |
| Clang/clang-tidy | 22.1.8 | Optional formatting and static analysis |
| `qt6-base` | 6.11.2-3 | Core, D-Bus, Widgets, and Test; pulls in `dbus` for `dbus-run-session` |
| `qt6-tools` | 6.11.2-1 | Qt development tools |
| `kcoreaddons` | 6.29.0-1 | Application metadata and version info |
| `ki18n` | 6.29.0-1 | Translatable UI strings |
| `kconfig` | 6.29.0-1 | Planned settings backend |
| `kio` | 6.29.0-2 | Dolphin file-item action API |
| `kxmlgui` | 6.29.0-1 | Planned KDE UI integration |
| `extra-cmake-modules` | 6.29.0-1 | KDE build and plugin installation paths |

`extra-cmake-modules` 6.29.0-1 was installed when Phase 2 began. The Dolphin plugin uses ECM's
`KDEInstallDirs6` and `KDECMakeSettings` modules for the standard `kf6/kfileitemaction` build and
installation layout. The API assessment is recorded in
[dolphin-context-menu.md](dolphin-context-menu.md).

The repository CI definition uses an up-to-date Arch Linux container. Unlike this Manjaro workstation,
that container installs declared dependencies into a clean image and uses a fixed job configuration;
successful local checks do not substitute for a hosted CI run.

## Verification commands

```bash
pacman -Q plasma-desktop plasma-workspace dolphin qt6-base qt6-tools
pacman -Q extra-cmake-modules kcoreaddons ki18n kconfig kio kxmlgui
pacman -Q gettext cmake git gcc ninja clang
```

No system packages were installed or modified during the initial bootstrap. Phase 2 added only the
verified `extra-cmake-modules` build dependency. The current suite builds and passes 11 CTest tests,
including two Dolphin plugin tests, on this environment and in the hosted Arch Linux CI container.
