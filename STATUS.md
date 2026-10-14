# Project Status

## Current phase

Phase 2 — Dolphin context menu.

## Resuming work

Last updated 2026-10-13.

- Published: `v0.1.0` provides the Git core and read-only inspector.
- Integrated in local `main` (`d087776`): minimal Dolphin action, context cache, experimental
  service, and asynchronous plugin client, through PRs #9 and #13–#15.
- Implemented and natively verified in `feature/repository-aware-actions` (`cfd04ff`):
  repository-aware menus for PR [#16](https://github.com/luks95/linuxgitshell/pull/16). The previous
  handoff records passing CI and an attached verification summary and screenshot. The remote PR
  state was not rechecked during this documentation review. Review and merge remain the recorded
  Phase 2 integration gate; the roadmap now explicitly assigns later actions to their feature phases.
- This branch (`docs/overlay-status-design`, from `main`) contains the Phase 3 design and the
  documentation reconciliation, with no implementation changes. The earlier design/handoff commits
  are local; this review adds working-tree edits. No push or PR creation was performed in this review.
- The overlay design now specifies snapshot coverage, initial replies, invalidation, observation
  leases, and provisional refresh/bounds defaults. These are proposed implementation contracts,
  not implemented capabilities. Native repaint verification and large-repository measurements
  remain Phase 3 validation gates; they do not block the isolated status-index model.
- Planned increments: `RepositoryStatusIndex` (Git-free, tested), daemon status provider with
  experimental D-Bus methods, overlay plugin with first icon set, native verification and `v0.2.0`.

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
- Hosted CI passing configure, build, all eight `v0.1.0` tests, static analysis, formatting, secret checks,
  and pull-request dependency review for the final release changes on the Arch Linux container.
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
- Public [`v0.1.0`](https://github.com/luks95/linuxgitshell/releases/tag/v0.1.0) release from the
  immutable annotated tag at `212a64b`, with versioned notes, source archive, and SHA-256 checksum.
- Release artifact automation validated the exact tag, static-analysis and Release builds, all eight
  tests, staged installation, application version, repository checks, and an offline rebuild from
  the generated archive. The downloaded workflow and public release artifacts also passed checksum
  verification locally.
- Public GitHub repository with the documented description and a protected `main` synchronized with
  the local checkout.
- Native Plasma 6 Wayland verification for candidate `4419b3c`, including repository state,
  credential redaction, reload, failure handling, missing-path guidance, and the installed Spanish
  catalog.
- GitHub topics, documentation link, Discussions, private vulnerability reporting, dependency and
  secret alerts, push protection, web commit sign-off, and protected-branch rules configured for the
  public repository.
- KF6 Dolphin context-menu API and dependency plan validated against Dolphin 26.08.0 and KIO 6.29.0,
  including the synchronous plugin boundary and standard `kf6/kfileitemaction` installation path.
- Thin KF6 file-item action plugin for a single local selection, with translated text and external
  `linuxgitshell` launch through a separate process argument. Empty, remote, and multiple selections
  produce no action, and the plugin performs no Git work inside Dolphin.
- Eleven local tests, including real plugin-factory loading, metadata and action policy checks, and
  exact transfer of a path containing spaces, Unicode, and a newline to an isolated launch helper.
- Development install/removal guidance, a staged `/usr` layout check, and a Dolphin manual-test
  checklist for the first context-menu increment.
- Phase 2 plugin merged by [PR #9](https://github.com/luks95/linuxgitshell/pull/9), closing issue #8;
  the final `main` CI run passed its build, 11 tests, Clang/`clang-tidy`, formatting, and secret checks.
- Git-free `libs/repositorycontext` snapshot model and bounded cache implementing the warm/cold/stale,
  size, freshness, generation, and invalidation rules of `docs/repository-context-cache.md`, with
  deterministic tests for normal, bare, linked-worktree, submodule, outside-repository, unusual-path,
  eviction, and multiple-selection cases.
- Experimental `linuxgitshell-daemon` session service that resolves repository context with
  `RepositoryDiscovery` outside Dolphin and publishes it over
  `org.linuxgitshell.Experimental.Context1` (contract in `dbus/`), with D-Bus activation, random
  per-instance generations, bounded requests, a 10-second discovery timeout, and a service cache.
  The service exits when its session bus disconnects.
- Non-blocking context client in the Dolphin plugin: `actions()` only reads a process-wide
  in-memory cache and queues cold or stale paths; requests leave from the event loop through
  asynchronous D-Bus calls that activate the service, and replies warm the cache for later menus.
  The visible menu is unchanged. Seventeen local tests pass; the plugin test loads the real module,
  proves that building menus neither runs Git nor contacts the service before returning, and
  measured `actions()` at p50 0.015 ms, p95 0.03 ms, and max 1.8 ms offscreen in a Debug build.
- Repository-local development installation verified with the expected binary, plugin, and Spanish
  catalog, followed by an isolated offscreen Dolphin startup with temporary D-Bus/XDG state.

## In progress

- Review and merge of [#16](https://github.com/luks95/linuxgitshell/pull/16), then review of the
  Phase 3 overlay design.
- Documentation reconciliation completed on this branch: phase ownership, branch-specific
  implementation evidence, dependency minimums, path limits, and the revised overlay contract.
  Verification: `git diff --check`, local link destinations in 25 Markdown files, fenced-block
  checks, `check-secrets`, and the proposed Git collectors in an isolated temporary repository.
  No application code changed; the application test suite and native UI checks were not rerun.

## Known issues

- Mutating user-facing Git operations are not implemented yet.
- Repository-aware Dolphin actions are implemented in the #16 branch but are not integrated in
  this checkout's `main` base. Overlays, watchers, and the stable `Daemon1` D-Bus API are not
  implemented; the context interface remains experimental.
- A repository root whose path contains a newline is reported as a discovery error, because
  `RepositoryDiscovery` reads Git's line-based path output.
- The native right-click checklist and latency results are recorded in the #16 branch, including
  its `docs/dolphin-context-menu.md` and screenshot. The older checklist in this checkout describes
  the minimal plugin; it is not evidence that the later verification is still outstanding.

## Next steps

1. Review and merge #16; update this file and the roadmap to mark Phase 2 complete.
2. Review the revised overlay contract and carry this documentation forward after #16 integration,
   preserving its native verification evidence when reconciling overlapping documentation changes.
3. Implement `RepositoryStatusIndex` with unit tests, then the daemon status provider.
