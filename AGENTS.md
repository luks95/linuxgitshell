# Repository Guidelines

## Project Structure & Module Organization

The repository contains the master specification (`LinuxGitShell-Codex.md`), execution plan
(`roadmap-checklist.md`), reusable Git code under `libs/gitcore/`, the Git-free repository-context
snapshot cache under `libs/repositorycontext/`, the desktop inspector under
`gui/app/`, the first file-item action plugin under `integrations/dolphin/contextmenu/`, and
automated checks under `tests/`. Add the session service under `daemon/`, D-Bus contracts under
`dbus/`, artwork under `icons/`, and distribution files under `packaging/` only when their first real
implementation is ready. Do not create empty placeholder directories.

Keep Dolphin plugins thin. Git execution, caching, watchers, and application windows belong in reusable services, not inside Dolphin's process.

## Build, Test, and Development Commands

Use an out-of-source debug build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --build build --target format-check
cmake --build build --target check-secrets
./build/linuxgitshell /path/to/repository
```

The first command configures Qt 6/KF6/ECM dependencies, the second compiles, and CTest runs all
registered tests, including real plugin-factory loading. Configure with an explicit install prefix
before testing Dolphin integration; ECM calculates the plugin directory at configure time. Do not
install missing system packages until their names and installed versions have been checked with
`pacman` and the need has been explained.

## Coding Style & Naming Conventions

Use C++20, four-space indentation, RAII, const correctness, and smart pointers where ownership is not implicit. Name types in `PascalCase`, functions and variables in `camelCase`, and files after their principal type (for example, `GitProcessRunner.cpp`). Keep parsing and models independent of widgets. Pass Git arguments separately to `QProcess`; never assemble commands through `shell -c`. Run the configured `format-check` target for C++ changes.

## Testing Guidelines

Register tests with CTest. Prioritize parsers, repository discovery, paths, config, status models, and
Dolphin selection boundaries. Integration tests must create temporary Git repositories and cover
clean, modified, staged, untracked, ignored, renamed, and conflicted states. Plugin tests must load
the built module rather than replacing it with a mock. Tests must never inspect or alter a
contributor's personal repositories.

## Commit & Pull Request Guidelines

Use the established short, imperative subjects such as `Add porcelain v2 status parser`; keep each
commit focused and include a DCO sign-off. Pull requests should explain the behavior change, link
relevant issues, report configure/build/test results, and include screenshots for UI changes. Update
`STATUS.md`, `CHANGELOG.md`, documentation, and translations when applicable.

## Security & Configuration

Never log credentials, tokens, private keys, or credential-bearing URLs. Respect XDG paths and KConfig. Require explicit confirmation for destructive Git operations and preserve the original Git output for diagnostics after sanitization.
