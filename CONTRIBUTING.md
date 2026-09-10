# Contributing to LinuxGitShell

LinuxGitShell is in its bootstrap phase. Small, focused changes that preserve the separation
between Git services, models, IPC, user interfaces, and Dolphin plugins are welcome.

## Development setup

Use an out-of-source debug build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Do not install system packages without first checking their names and installed versions with
`pacman`. The validated environment is documented in `docs/development-environment.md`.

## Coding and tests

- Use C++20, four-space indentation, RAII, const correctness, and explicit ownership.
- Keep parsing and models independent of Qt Widgets.
- Pass the executable and every Git argument separately to `QProcess`; never use `shell -c`.
- Add CTest coverage proportional to the change. Integration tests must use temporary repositories.
- Run `cmake --build build --target format-check` before submitting C++ changes.
- Configure with `-DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_CLANG_TIDY=clang-tidy` to run the
  configured static checks while building.

UI text must use `i18n()` or the appropriate KI18n API. Update `po/es/linuxgitshell.po` when visible
source strings change; see `docs/translations.md`.

## Issues and pull requests

Use an issue template when one fits. A pull request should explain the behavior change, link its
issue or design discussion, report configure/build/test results, and include screenshots for visual
changes. Update `STATUS.md`, `CHANGELOG.md`, documentation, and translations when applicable.

Keep commits focused and use short imperative subjects, such as `Add porcelain v2 status parser`.
Do not include credentials, private repository data, or credential-bearing URLs in commits or logs.

## Developer Certificate of Origin

Contributions use the [Developer Certificate of Origin 1.1](https://developercertificate.org/)
instead of a CLA. Sign off each commit to certify that you have the right to contribute it:

```bash
git commit --signoff
```

By contributing, you agree that your contribution is licensed under the MIT License.
