# LinuxGitShell

LinuxGitShell aims to provide a native, graphical Git experience for Linux, starting with KDE Plasma 6 and Dolphin on Manjaro/Arch Linux. The project is currently building its Git Core; the application can inspect a repository asynchronously and show its root, branch, status summary, and read-only configuration. Dolphin integration and mutating Git workflows are not available yet.

See [LinuxGitShell-Codex.md](LinuxGitShell-Codex.md) for the product specification and [roadmap-checklist.md](roadmap-checklist.md) for the implementation plan.

## Current requirements

The bootstrap has been validated with:

- C++20 compiler (GCC 16.2.1)
- CMake 4.4.3
- Qt 6.11.2 (`Core`, `Widgets`, and `Test`)
- KDE Frameworks 6.29.0 (`CoreAddons` and `I18n`)
- Gettext 1.0 (translation catalog compilation)
- Git 2.55.0
- Ninja 1.13.2 or another CMake-supported build tool

On Manjaro/Arch, verify installed packages before changing the system:

```bash
pacman -Q qt6-base kcoreaddons ki18n gettext cmake gcc ninja
```

`extra-cmake-modules` 6.29.0 is available in the repositories but is not installed in the validated environment. The current bootstrap does not require it; later KDE integration is expected to use it.

The complete environment snapshot is recorded in [docs/development-environment.md](docs/development-environment.md).

## Build and test

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Check formatting before submitting C++ changes:

```bash
cmake --build build --target format-check
```

Clang and `clang-tidy` are optional local quality tools; the CI quality job uses both.

Run the application shell with:

```bash
./build/linuxgitshell /path/to/repository
```

The optional path is discovered asynchronously. The window displays repository metadata, a working
tree summary, configuration values and origins, and diagnostic Git output when inspection fails.
Use [docs/manual-smoke-test.md](docs/manual-smoke-test.md) for the KDE/Wayland verification checklist.

## Local installation and removal

Use a disposable user prefix while developing:

```bash
cmake --install build --prefix "$PWD/install"
./install/bin/linuxgitshell
```

Remove that local installation by deleting only the repository's `install/` directory. System-wide installation is not needed during bootstrap.

## Contributing and license

LinuxGitShell is licensed under the [MIT License](LICENSE). Contributions use DCO sign-off rather
than a CLA.

Read [AGENTS.md](AGENTS.md) and [CONTRIBUTING.md](CONTRIBUTING.md) before making changes. Project
governance, conduct, security reporting, support, licensing details, and translation workflow are
documented in the corresponding top-level files and under `docs/`.
