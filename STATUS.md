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
- Hosted CI passing configure, build, all eight tests, static analysis, formatting, and secret checks
  for the final candidate `2522135` on the Arch Linux container.
- Asynchronous `GitProcessRunner` validated with the native Linux Qt/KF6 toolchain and deterministic
  tests for cancellation, timeouts, process failures, environment handling, and unusual paths.
- Repository discovery from directories or files, with typed results for normal repositories, bare
  repositories, linked worktrees, and submodules.
- Repository metadata for branch or detached HEAD, upstream, ahead/behind counts, remote names, and
  merge, rebase, cherry-pick, revert, or bisect operations in progress.
- Isolated discovery tests covering `.git` directories and files, nested paths, symbolic links,
  separate Git metadata, paths longer than 1,400 characters, spaces, Unicode, case-distinct names,
  names beginning with a hyphen, divergent upstreams, and a real merge conflict.
- Strict, UI-independent parser for `git status --porcelain=v2 -z`, with separate index and working
  tree states, original rename/copy paths, conflict stages, submodule flags, and branch headers.
- Asynchronous status reader using `--no-optional-locks`, typed execution/parse errors, opt-in ignored
  files, configurable rename detection, and preserved original Git output.
- Temporary-repository integration coverage for clean, modified, staged, combined, untracked,
  ignored, added, deleted, renamed, Unicode/newline paths, bare repositories, and real conflicts.
- Read-only asynchronous Git config inspection preserving scope, origin, repeated keys, empty and
  multiline values, with credential redaction in all returned diagnostics.
- Isolated config integration coverage for system, global, local, and worktree scopes, including a
  byte-for-byte assertion that reading does not modify repository configuration.
- Functional application view accepting a repository path and showing its root, branch, type,
  asynchronous status summary, and read-only configuration entries with their origins.
- Reload support, localized typed failures, inspectable Git diagnostics, and an offscreen application
  integration test covering clean, changed, and invalid repositories.
- `v0.1.0` candidate version, draft release notes, a maintainer checklist, and tag-triggered source
  archive/checksum automation.
- Public GitHub repository with the documented description and `main` synchronized with the local
  candidate through `2522135` as of 2026-09-13.
- Native Plasma 6 Wayland verification for candidate `4419b3c`, including repository state,
  credential redaction, reload, failure handling, missing-path guidance, and the installed Spanish
  catalog.
- GitHub topics, documentation link, Discussions, private vulnerability reporting, dependency and
  secret alerts, push protection, web commit sign-off, and protected-branch rules configured for the
  public repository.

## In progress

- Merge the final release-record pull request after its required checks pass.
- Publish the `v0.1.0` tag and release artifacts.

## Known issues

- `extra-cmake-modules` is available but not installed; the bootstrap avoids requiring it.
- Mutating user-facing Git operations are not implemented yet.
- Dolphin integration, overlays, daemon, and D-Bus are not implemented.

## Next steps

1. Merge the final release-record pull request after `build`, `quality`, and `dependency-review` pass.
2. Publish the annotated tag, source archive, checksum, and release notes.
