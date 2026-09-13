# Changelog

All notable user-visible changes to LinuxGitShell will be documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and releases follow semantic versioning.

## [Unreleased]

### Added

- KF6 Dolphin file-item action plugin that offers a translated external-launch action for one local
  selection while keeping Git execution and application windows outside Dolphin's process.
- Unit and integration coverage for conservative selection handling, plugin metadata and factory
  loading, and shell-free transfer of unusual paths as one process argument.
- Development installation, clean removal, staged system-layout verification, and a manual Dolphin
  smoke-test checklist.

### Changed

- Project documentation now reflects the public `v0.1.0` release, the merged Phase 2 plugin, the
  eleven-test suite, current dependencies, and the proposed non-blocking repository-context cache.

## [0.1.0] - 2026-09-13

### Added

- MIT license and initial open source governance, contribution, conduct, security, and support policies.
- Spanish translation catalog and documented translation workflow.
- CI definitions for build, tests, formatting, static analysis, and basic secret detection.
- Asynchronous Git process runner with cancellation, timeout, typed results, raw output capture, and
  credential-safe diagnostic argument sanitization.
- Deterministic process-runner coverage for failure modes, environment handling, and unusual paths.
- Asynchronous repository discovery for normal repositories, bare repositories, linked worktrees,
  and submodules, including typed failures and preserved Git diagnostics.
- Repository metadata for the current branch, detached HEAD, upstream divergence, remote names, and
  merge, rebase, cherry-pick, revert, or bisect operations in progress.
- Isolated integration tests for discovery from nested files, symbolic links, `.git` files, and
  paths containing spaces, Unicode, and leading hyphens, plus upstream divergence and a real merge
  conflict.
- Path-safe repository discovery through file symlinks, deep paths longer than 1,400 characters,
  case-distinct repository names, and worktrees with Git metadata stored separately.
- UI-independent parser and typed model for NUL-delimited porcelain v2 status, including separate
  index/working-tree states, renames and copies, conflicts, submodules, ignored files, and branch
  headers.
- Asynchronous, non-locking status reader with opt-in ignored files, rename detection, typed errors,
  preserved Git output, and integration coverage against isolated temporary repositories.
- Read-only asynchronous Git config reader preserving scope, origin, repeated keys, and multiline
  values, with sanitized diagnostics and isolated system/global/local/worktree integration tests.
- Functional read-only application view for repository metadata, status counts, configuration
  origins and values, reloads, and inspectable localized Git failure diagnostics.
- Offscreen application integration tests for clean and changed temporary repositories and
  non-repository failures, plus a KDE/Wayland manual smoke-test checklist.
- Reproducible `v0.1.0` release instructions, release notes, tag/version validation, and automated
  generation of a tagged source archive with SHA-256 checksums.

## [0.0.1] - 2026-09-09

### Added

- Initial C++20, Qt 6, and KDE Frameworks 6 application shell.
- Reusable `gitcore` bootstrap library, logging category, and CTest test runner.

[Unreleased]: https://github.com/luks95/linuxgitshell/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/luks95/linuxgitshell/releases/tag/v0.1.0
