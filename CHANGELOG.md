# Changelog

All notable user-visible changes to LinuxGitShell will be documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and releases follow semantic versioning.

## [Unreleased]

### Added

- Latency measurements of the overlay daemon on a 100k-file fixture (cold answer 1.4 s, warm initial
  records about 35 ms, edit-to-update 3.0-12.6 s, about 5% of one core idle), recorded with their
  caveats in `docs/dolphin-overlays.md`.
- Native verification record for the Dolphin overlays (isolated Dolphin on Qt's `vnc` platform): five
  states in three views, live updates, expiry, daemon restart recovery, and the `Enabled` and
  `RecursiveFolders` settings, with screenshots under `docs/images/`.
- Pure `RepositoryStatusIndex` for the upcoming overlays: separate index/work-tree details,
  tracked membership, directory propagation, ignored-prefix exceptions, submodule flags, bounded
  atomic snapshot replacement, explicit freshness and revision-based invalidation. The daemon
  provider is built on it; visible overlays and the Dolphin overlay plugin are not implemented yet.
- Bounded Git process capture: `GitProcessRequest::maxOutputBytes` limits stdout plus stderr while
  reading, drops the excess, stops the process, and reports `OutputLimitExceeded`.
- `GitInventoryReader` for the complete tracked-path inventory (`ls-files --cached -z`) with
  deduplication and no partial results after a timeout, overflow, or failure.
- Experimental overlay status provider in the daemon: per-repository (never per-icon) collection of
  status plus inventory under a shared deadline and output cap, bounded concurrency, debounced and
  polled refresh with backoff, 90-second observation leases, 30-second snapshot freshness, nested
  repository and submodule ownership, and `Unavailable` answers for bare, sparse-checkout,
  pseudo-filesystem, and network or unclassified locations.
- Experimental `org.linuxgitshell.Experimental.Overlays1` D-Bus service served by the same daemon,
  with its own activation file: `RequestOverlays`, `ReleaseOverlays`, `RefreshRepository`, and
  unicast `OverlayResults`/`OverlaysChanged` signals, a shared `a{sv}` record encoding that rejects
  unknown values, and release of a client's observations when it leaves the bus.
- Dolphin icon-overlay plugin (`KOverlayIconPlugin`) with a bounded process-wide cache, deferred
  and batched requests, observation renewal, backoff, generation and revision checks, and bus-verified
  sender authentication. Paint callbacks only read memory: tests load the real module and show that
  10,000 cold callbacks perform no Git or D-Bus work. Overlays are on by default and can be disabled
  with `Enabled=false` in the `[Overlays]` group of `linuxgitshellrc`; see `docs/dolphin-overlays.md`.
- `RecursiveFolders` setting (default on) for the daemon's folder-state propagation, read at daemon
  start-up and independent of whether overlays are enabled in Dolphin.
- Original MIT-licensed overlay emblems for clean, modified, added, conflict, and untracked items,
  distinguished by shape and legible at 16, 22, and 32 px on light and dark themes.
- KF6 Dolphin file-item action plugin that offers a translated external-launch action for one local
  selection while keeping Git execution and application windows outside Dolphin's process.
- Unit and integration coverage for conservative selection handling, plugin metadata and factory
  loading, and shell-free transfer of unusual paths as one process argument.
- Development installation, clean removal, staged system-layout verification, and a manual Dolphin
  smoke-test checklist.
- Git-free repository-context snapshot model and bounded LRU cache with warm/cold/stale lookups,
  separate positive and negative freshness, service-generation resets, repository and subtree
  invalidation, and same-repository multiple-selection checks, ready for the Dolphin client.
- Experimental `linuxgitshell-daemon` session service exposing
  `org.linuxgitshell.Experimental.Context1` over D-Bus, with activation file, extensible `a{sv}`
  snapshots, per-instance generations, bounded requests and queue, concurrent discovery with a
  timeout, a service-side cache, and no path logging. Tests cover normal, bare, linked-worktree,
  submodule, outside, missing, unusual, cached, deduplicated, and timed-out paths and the real
  daemon on a private session bus. The service exits when its session bus disconnects.
- Non-blocking repository-context client in the Dolphin plugin. Menus read only an in-memory cache,
  and cold paths are requested asynchronously from the event loop through D-Bus activation. Tests
  load the real module and prove that building a menu neither runs Git nor contacts the service.
- Repository-aware Dolphin menu: `LinuxGitShell` ▸ `Show Status` inside repositories (including
  bare ones), disabled notices for operations in progress, the repository root for several items in
  one repository, and no entry outside repositories or across repositories. Spanish translations
  cover the new entries.
- Optional Dolphin menu timing diagnostics, disabled by default, reporting menu kind, selection
  count, and elapsed nanoseconds without logging paths.

### Changed

- LinuxGitShell window and action labels default to English while preserving Dolphin's locale.
- The Dolphin selection helper is now a policy for single and multiple repository selections.
- Project documentation distinguishes the published release, integrated Phase 2 context service
  and client, the 18-test suite (now 28 with the overlay provider and plugin work), and repository-aware
  menus integrated through #16.
  Deferred menu actions have explicit target phases; the proposed overlay contract specifies
  snapshot coverage, initial replies, invalidation, and bounded background refresh.

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
