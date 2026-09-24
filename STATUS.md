# Project Status

## Current phase

Phase 2 — Dolphin context menu.

## Resuming work

Last updated 2026-09-24. The repository-context work for
[issue #12](https://github.com/luks95/linuxgitshell/issues/12) is complete in four stacked pull
requests. Each targets the previous branch and has passing CI. Merge them into `main` in this order,
retargeting the next pull request to `main` after each merge:

| Order | Pull request | Branch | Content |
| --- | --- | --- | --- |
| 1 | [#13](https://github.com/luks95/linuxgitshell/pull/13) | `feature/repository-context-cache` | Snapshot model and bounded cache in `libs/repositorycontext/` |
| 2 | [#14](https://github.com/luks95/linuxgitshell/pull/14) | `feature/context-service` | `linuxgitshell-daemon` with `org.linuxgitshell.Experimental.Context1` |
| 3 | [#15](https://github.com/luks95/linuxgitshell/pull/15) | `feature/plugin-context-client` | Non-blocking context client in the Dolphin plugin |
| 4 | [#16](https://github.com/luks95/linuxgitshell/pull/16) | `feature/repository-aware-actions` | `LinuxGitShell` ▸ `Show Status` menu and this documentation |

Decisions already taken:

- The context service lives in `daemon/` as the first increment of the Phase 4 daemon and uses the
  experimental bus name `org.linuxgitshell.Experimental.Context1`, not the stable `Daemon1` name.
- The menu offers only actions the application implements. Today that is `Show Status`, which opens
  the read-only inspector. Other roadmap actions are added when their application features land.

The remaining Phase 2 work is manual and needs a native Plasma Wayland session:

1. Install the development prefix, start the daemon, and restart Dolphin from the same terminal, as
   described in [`docs/dolphin-context-menu.md`](docs/dolphin-context-menu.md#development-installation).
   A Dolphin started from the Plasma launcher does not load the development plugin.
2. Complete the manual checklist in that document, attach screenshots to #16, and record native
   p50/p95/max `actions()` latency.
3. Evaluate the Phase 2 exit criteria in `roadmap-checklist.md`, then plan the `v0.2.0` overlays
   work (Phase 3).

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
  Seventeen local tests pass; the plugin test loads the real module,
  proves that building menus neither runs Git nor contacts the service before returning, and
  measured `actions()` at p50 0.015 ms, p95 0.03 ms, and max 1.8 ms offscreen in a Debug build.
- Repository-aware Dolphin menus for the application's current features: a warm snapshot inside a
  repository, bare repositories included, shows `LinuxGitShell` ▸ `Show Status` with disabled
  notices for operations in progress; a directory outside a repository shows nothing; several items
  in the same repository open the repository root; cold, failed, remote, mixed, or cross-repository
  selections degrade conservatively. Menu entries and notices are translated into Spanish.
- Repository-local development installation verified with the expected binary, plugin, and Spanish
  catalog, followed by an isolated offscreen Dolphin startup with temporary D-Bus/XDG state.

## In progress

- Complete interactive Dolphin verification of the repository-aware menu, tracked by [issue #12](https://github.com/luks95/linuxgitshell/issues/12).

## Known issues

- Mutating user-facing Git operations are not implemented yet.
- `Commit`, `Pull`, `Push`, `Show Log`, `Settings`, `Git Clone`, and `Create repository here` are
  not offered because the application does not implement them yet.
- Overlays, watchers, and the stable `Daemon1` D-Bus API are not implemented; the context interface
  is experimental.
- The first right-click on a new path shows the generic action; repository entries appear once the
  asynchronous reply has arrived.
- A repository root whose path contains a newline is reported as a discovery error, because
  `RepositoryDiscovery` reads Git's line-based path output.
- The plugin has automated factory/loading coverage and an isolated Dolphin startup, but its manual
  right-click checklist has not yet been completed in a native interactive session.

## Next steps

1. Run the documented checklist with the development plugin loaded by Dolphin on Plasma Wayland.
2. Record native p50/p95/max `actions()` latency in Dolphin and complete the Phase 2 exit criteria.
3. Add further menu actions as the corresponding application features land.
