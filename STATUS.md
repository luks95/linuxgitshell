# Project Status

## Current phase

Phase 1 — Git Core.

## Working

- Manjaro/Arch development environment inventoried.
- CMake project configured for C++20, Qt 6, and KDE Frameworks 6.
- Minimal `linuxgitshell` Qt Widgets application with KI18n-ready text.
- Compiled and tested Spanish KI18n catalog alongside the English source strings.
- Initial reusable `gitcore` library and logging category.
- Qt Test runner integrated with CTest, including a translation catalog test.
- Out-of-source build and repository-local installation flow documented.
- MIT license, DCO contribution workflow, governance, conduct, security, and support documents.
- GitHub-compatible issue/PR templates and CI workflow for build, tests, formatting, static analysis,
  and common secret patterns.

## In progress

- Asynchronous `GitProcessRunner` with typed completion states, safe cancellation escalation,
  opt-in timeout, original output capture, and credential-safe argument sanitization.
- Deterministic process-runner tests for failure modes, environment handling, and unusual paths.
- Validate the process-runner implementation with the native Linux Qt/KF6 toolchain and hosted CI.
- Add application-layer translations for common typed process errors.

## Known issues

- `extra-cmake-modules` is available but not installed; the bootstrap avoids requiring it.
- A real KDE/Wayland launch could not be verified from the restricted execution environment; the
  application remained running under Qt's offscreen platform until the smoke-test timeout.
- The current Windows execution environment has no CMake/Qt toolchain, so the new runner tests have
  not yet been compiled or executed locally.
- Repository discovery, status parsing, and user-facing Git operations are not implemented yet.
- Dolphin integration, overlays, daemon, and D-Bus are not implemented.
- The repository is initialized on `main` with a signed-off foundation commit and an `origin` remote;
  publication settings and branch protection have not been verified.

## Next steps

1. Verify the published repository, configure private reporting, and protect `main`.
2. Confirm the workflow passes on the public forge and perform the KDE/Wayland smoke test interactively.
3. Begin repository discovery after the process-runner build and tests pass.
