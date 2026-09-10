# Project Status

## Current phase

Phase 0 — Open source foundation and technical bootstrap.

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

- Publish the project to an open forge and add the real private security/contact channel.
- Run and enforce the new CI workflow from a clean hosted checkout.

## Known issues

- `extra-cmake-modules` is available but not installed; the bootstrap avoids requiring it.
- A real KDE/Wayland launch could not be verified from the restricted execution environment; the
  application remained running under Qt's offscreen platform until the smoke-test timeout.
- Repository discovery and all Git operations are intentionally deferred to Phase 1.
- Dolphin integration, overlays, daemon, and D-Bus are not implemented.
- The repository is initialized on `main` with a signed-off initial foundation commit; no remote exists yet.

## Next steps

1. Publish the repository, configure private reporting, and protect `main`.
2. Confirm the workflow passes on the public forge and perform the KDE/Wayland smoke test interactively.
3. Begin Phase 1 with the asynchronous Git process runner.
