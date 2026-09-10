# Repository Guidelines

## Project Structure & Module Organization

The repository currently contains the master specification (`LinuxGitShell-Codex.md`) and execution plan (`roadmap-checklist.md`). Evolve it incrementally toward the documented layout: shared code under `libs/` (`gitcore`, models, diff, and IPC), the session service under `daemon/`, Dolphin adapters under `integrations/dolphin/`, desktop views under `gui/`, D-Bus contracts under `dbus/`, artwork under `icons/`, distribution files under `packaging/`, and automated checks under `tests/`. Do not create empty placeholder directories.

Keep Dolphin plugins thin. Git execution, caching, watchers, and application windows belong in reusable services, not inside Dolphin's process.

## Build, Test, and Development Commands

Use an out-of-source debug build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/linuxgitshell /path/to/repository
```

The first command configures Qt 6/KF6 dependencies, the second compiles, and CTest runs all registered tests. Do not install missing system packages until their names and installed versions have been checked with `pacman` and the need has been explained.

## Coding Style & Naming Conventions

Use C++20, four-space indentation, RAII, const correctness, and smart pointers where ownership is not implicit. Name types in `PascalCase`, functions and variables in `camelCase`, and files after their principal type (for example, `GitProcessRunner.cpp`). Keep parsing and models independent of widgets. Pass Git arguments separately to `QProcess`; never assemble commands through `shell -c`. Apply the repository's formatter once its configuration is added.

## Testing Guidelines

Register tests with CTest. Prioritize parsers, repository discovery, paths, config, and status models. Integration tests must create temporary Git repositories and cover clean, modified, staged, untracked, ignored, renamed, and conflicted states. Tests must never inspect or alter a contributor's personal repositories.

## Commit & Pull Request Guidelines

No Git history exists yet, so no established commit convention can be inferred. Use short, imperative subjects such as `Add porcelain v2 status parser`; keep each commit focused. Pull requests should explain the behavior change, link relevant issues, report configure/build/test results, and include screenshots for UI changes. Update `STATUS.md`, `CHANGELOG.md`, documentation, and translations when applicable.

## Security & Configuration

Never log credentials, tokens, private keys, or credential-bearing URLs. Respect XDG paths and KConfig. Require explicit confirmation for destructive Git operations and preserve the original Git output for diagnostics after sanitization.
