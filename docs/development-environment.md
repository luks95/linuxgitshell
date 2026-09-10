# Development Environment

This snapshot records the environment used for the initial bootstrap on 2026-09-09. It is evidence of a working development setup, not the final support matrix.

## Platform

| Component | Detected version |
| --- | --- |
| Distribution | Manjaro Linux, rolling |
| KDE Plasma (`plasma-desktop`) | 6.7.4-1 |
| Plasma Workspace | 6.7.4-3 |
| Dolphin | 26.08.0-5 |
| Qt (`qt6-base`) | 6.11.2-3 |
| KDE Frameworks | 6.29.0 |
| LinuxGitShell | 0.0.1 bootstrap |

## Toolchain and packages

| Package/tool | Detected version | Bootstrap role |
| --- | --- | --- |
| GCC | 16.2.1 | C++20 compiler |
| CMake | 4.4.3 | Configure and generate |
| Ninja | 1.13.2 | Build executor |
| Git | 2.55.0 | Required external Git implementation |
| Gettext | 1.0 | Spanish translation catalog compilation |
| Clang/clang-tidy | 22.1.8 | Optional formatting and static analysis |
| `qt6-base` | 6.11.2-3 | Core, Widgets, and Test |
| `qt6-tools` | 6.11.2-1 | Qt development tools |
| `kcoreaddons` | 6.29.0-1 | Application metadata and version info |
| `ki18n` | 6.29.0-1 | Translatable UI strings |
| `kconfig` | 6.29.0-1 | Planned settings backend |
| `kio` | 6.29.0-2 | Planned KDE/Dolphin integration |
| `kxmlgui` | 6.29.0-1 | Planned KDE UI integration |

`extra-cmake-modules` is not installed. `pacman` reports version 6.29.0-1 in the configured repositories. Direct KF6 CMake package files are sufficient for the current bootstrap; ECM will be reassessed before Dolphin integration and translation catalog generation.

The repository CI definition uses an up-to-date Arch Linux container. Unlike this Manjaro workstation,
that container installs declared dependencies into a clean image and uses a fixed job configuration;
successful local checks do not substitute for a hosted CI run.

## Verification commands

```bash
pacman -Q plasma-desktop plasma-workspace dolphin qt6-base qt6-tools
pacman -Q kcoreaddons ki18n kconfig kio kxmlgui gettext cmake git gcc ninja clang
pacman -Ss '^extra-cmake-modules$'
```

No system packages were installed or modified during bootstrap.
