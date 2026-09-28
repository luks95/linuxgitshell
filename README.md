# LinuxGitShell

LinuxGitShell aims to provide a native, graphical Git experience for Linux, starting with KDE
Plasma 6 and Dolphin on Manjaro/Arch Linux. The public
[`v0.1.0`](https://github.com/luks95/linuxgitshell/releases/tag/v0.1.0) release provides the tested
Git Core and a read-only repository inspector. Phase 2 now includes the first thin Dolphin
context-menu plugin on the development branch: a single local item can open the external
LinuxGitShell application. Repository-aware actions and mutating Git workflows are not available
yet.

## Current status

| Capability | State |
| --- | --- |
| Git process runner, repository discovery, status, and config inspection | Released in `v0.1.0` |
| Read-only Qt/KF6 repository inspector | Released in `v0.1.0` |
| Minimal Dolphin action for one local selection | Available on `main`, unreleased |
| Repository-aware menus and context cache | Cache and experimental context service implemented; plugin client tracked by [#12](https://github.com/luks95/linuxgitshell/issues/12) |
| Overlays, stable daemon/D-Bus API, and mutating Git workflows | Not implemented |

See [`STATUS.md`](STATUS.md) for verification evidence and the exact next steps.

## Documentation map

- Development: [environment](docs/development-environment.md),
  [Dolphin plugin](docs/dolphin-context-menu.md),
  [repository-context cache](docs/repository-context-cache.md),
  [path handling](docs/path-handling.md), and [translations](docs/translations.md).
- Verification and delivery: [application smoke test](docs/manual-smoke-test.md),
  [release process](docs/release-process.md), and [release notes](docs/releases/v0.1.0.md).
- Architecture and policy: [master specification](LinuxGitShell-Codex.md),
  [roadmap](roadmap-checklist.md), [D-Bus compatibility](docs/dbus-api-policy.md),
  [licensing](docs/licensing.md), and [project management](docs/project-management.md).
- Community: [contributing](CONTRIBUTING.md), [support](SUPPORT.md), [security](SECURITY.md),
  [governance](GOVERNANCE.md), and [code of conduct](CODE_OF_CONDUCT.md).

## Current requirements

The current development branch has been validated with:

- C++20 compiler (GCC 16.2.1)
- CMake 4.4.3
- Qt 6.11.2 (`Core`, `Widgets`, and `Test`)
- KDE Frameworks 6.29.0 (`CoreAddons`, `I18n`, and `KIO`)
- Extra CMake Modules 6.29.0
- Gettext 1.0 (translation catalog compilation)
- Git 2.55.0
- Ninja 1.13.2 or another CMake-supported build tool

On Manjaro/Arch, verify installed packages before changing the system:

```bash
pacman -Q qt6-base extra-cmake-modules kcoreaddons ki18n kio gettext cmake gcc ninja
```

`extra-cmake-modules` 6.29.0 is required by the Phase 2 Dolphin plugin for standard KDE build and
installation paths. See
[docs/dolphin-context-menu.md](docs/dolphin-context-menu.md) for the verified API and dependency
plan.

The complete environment snapshot is recorded in [docs/development-environment.md](docs/development-environment.md).

## Build and test

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The current suite registers 15 tests, including plugin metadata/factory loading, selection policy,
unusual-path process transfer, the bounded repository-context cache and its D-Bus encoding, the
context service against temporary repositories, the real daemon on a private session bus through
`dbus-run-session`, and Spanish translations.

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
Repository discovery behavior for symlinks, mount boundaries, long paths, and case sensitivity is
documented in [docs/path-handling.md](docs/path-handling.md).
The proposed non-blocking path-to-repository cache is documented in
[docs/repository-context-cache.md](docs/repository-context-cache.md).

Maintainers preparing a tagged version should follow
[docs/release-process.md](docs/release-process.md). Notes for the first development release are in
[docs/releases/v0.1.0.md](docs/releases/v0.1.0.md).

## Local installation and removal

Use a disposable user prefix while developing. Configure the prefix up front so ECM calculates the
matching plugin directory:

```bash
cmake -S . -B build-dolphin -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX="$PWD/install"
cmake --build build-dolphin -j
ctest --test-dir build-dolphin --output-on-failure
cmake --install build-dolphin
./install/bin/linuxgitshell
```

The Dolphin plugin needs the development environment described in
[docs/dolphin-context-menu.md](docs/dolphin-context-menu.md). Remove the local installation by
deleting only the repository's `install/` directory. A system-wide installation is not required for
the development loop.

For release-layout verification, configure a separate build with `-DCMAKE_INSTALL_PREFIX=/usr` and
install it below a `DESTDIR`; do not copy development files into `/usr` manually. Distribution
packaging is not available yet.

## Contributing and license

LinuxGitShell is licensed under the [MIT License](LICENSE). Contributions use DCO sign-off rather
than a CLA.

Read [AGENTS.md](AGENTS.md) and [CONTRIBUTING.md](CONTRIBUTING.md) before making changes. Project
governance, conduct, security reporting, support, licensing details, and translation workflow are
documented in the corresponding top-level files and under `docs/`.
