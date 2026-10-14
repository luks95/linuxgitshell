# Project Status

## Current phase

Phase 3 — Overlay status model.

## Resuming work

Last updated 2026-10-13.

- PR [#16](https://github.com/luks95/linuxgitshell/pull/16) merged as `ec85d2d` after review of
  `cfd04ff`, passing build/quality/dependency checks, and the attached native verification summary
  and screenshot. Phase 2 is complete; the published release remains `v0.1.0`.
- Branch `feature/repository-status-index` combines the integrated menu with the revised
  [overlay contract](docs/overlay-status-design.md). Documentation corrections are preserved in
  `17252eb`; no overlays/provider are implemented yet.
- Next implementation: pure `RepositoryStatusIndex` and deterministic tests. Native overlay repaint
  and large-repository measurements remain release gates for Phase 3.

## Historical Phase 2 verification

These checkpoints retain the evidence recorded before #16 merged. Their pending-action notes
are historical and are superseded by the integration state above.

Verification on 2026-09-28:

- Debug configure/build and development-prefix installation pass with the installed Qt/KF6 toolchain.
- All 17 development-build CTest tests pass outside the sandbox; private D-Bus tests need socket
  access unavailable inside it. Formatting and common-secret checks also pass.
- The real plugin test passes all 11 Qt Test cases with `QT_QPA_PLATFORM=wayland`, on a private
  session bus: `actions()` p50 0.034 ms, p95 0.046 ms, max 1.327 ms over 200 cold-cache calls.
  This is a native-backend test executable measurement, not a measurement inside Dolphin.
- A separate Dolphin process was started on Plasma Wayland with the development plugin prefix,
  private D-Bus/XDG state, and disposable repositories. Its accessibility tree could be inspected,
  but synthetic keyboard input did not open the context menu. No interactive checklist items,
  context-menu screenshots, or native Dolphin latency measurements were obtained. The test
  processes were stopped afterwards; the manual gate remains open.
- Follow-up: the plugin now exposes opt-in `linuxgitshell.dolphin.timing` diagnostics, disabled by
  default, to measure `actions()` inside Dolphin without logging paths. The development build,
  all 17 CTest tests, the real-module test with timing enabled, formatting, secret checks, and the
  Clang/clang-tidy plugin build pass. A temporary desktop-portal keyboard session was granted,
  but input did not reliably open the selected-item context menu. The native checklist and latency
  samples remain pending. The portal session and isolated test processes were closed, the temporary
  compositor script was unloaded, and accessibility preferences were restored to their original
  values. No native Dolphin menu samples have been claimed.

### Checkpoint on 2026-09-29

- Branch: `feature/repository-aware-actions`; published implementation commit: `eaa48cc`
  (`Add opt-in Dolphin menu timing diagnostics`, with DCO sign-off).
- [CI run 36503897725](https://github.com/luks95/linuxgitshell/actions/runs/36503897725)
  passes build/tests, Clang/clang-tidy, formatting, secret checks, and dependency review for that
  commit. PR #16 remains open against `main`; #13, #14, and #15 are already merged.
- Final local verification passed all 17 CTest tests. The real-module test also passed both with
  default logging (no timing lines) and with timing enabled (234 lines: 10 no-entry, 219 generic,
  and 5 repository decisions). Every timing line contained only kind, selection count, and elapsed
  nanoseconds. These are test-process diagnostics, not native Dolphin latency samples.
- After closing the automated-input attempt, a new isolated Dolphin window was prepared with the
  disposable `repo` directory selected and timing enabled, awaiting physical right-click checks.
  This preparation does not prove menu loading or behavior; no completed manual check has been
  confirmed.
- Resume with a first right-click, close the menu, wait for asynchronous discovery, then right-click
  the same selection again. Record the cold fallback and warm submenu before continuing the full
  checklist. Do not assume the temporary window or fixture survives; recreate an isolated test
  session if needed using the documented development installation.
- Do not merge #16 or close Phase 2 until the interactive checklist, context-menu screenshots,
  and separate cold/warm native latency measurements are recorded.

### Checkpoint on 2026-10-05

- Physical checks in disposable repositories on Plasma Wayland confirmed cold generic and warm
  `Show Status` menus for a root, subdirectory, file, and bare repository; an inspector launch for
  the selected repository; a disabled merge notice; no entry outside Git or across two
  repositories; and the repository root for two selected files. The user supplied a screenshot of
  the bare inspector. Dolphin stayed responsive without the daemon and showed a handled launch
  error when the development binary was absent from `PATH`; the user supplied an error screenshot.
- Native timing from the original development module: generic single-item decisions with the
  daemon available, n=9, p50 1.680 ms, p95/max 4.427 ms; warm single-item repository menus,
  n=24, p50 0.235 ms, p95 0.703 ms, max 0.910 ms. The cold p95 exceeds the design target of 2 ms.
  Samples include first calls and use nearest-rank percentiles. Details and limits are in
  [`docs/dolphin-context-menu.md`](docs/dolphin-context-menu.md#native-checkpoint-on-2026-10-05).
- The user requested English as LinuxGitShell's default language. Local changes select English in
  the inspector and only for the plugin's own labels, preserving Dolphin's locale. The rebuilt
  real-module test runs with a Spanish host locale and confirms English action labels. Configure,
  build, all 17 CTest tests, formatting, and common-secret checks pass locally.
- With the new build, a fresh Dolphin process remained responsive while an external Git wrapper
  delayed discovery by one second per call in a temporary repository with 2,000 tracked files.
  The user confirmed that the menu reached `Show Status` in English and supplied a
  [menu screenshot](docs/images/dolphin-repository-menu.png). Generic single-item decisions had
  n=3, p50 0.211 ms, p95/max 1.787 ms; warm repository menus had n=29, p50 0.173 ms, p95
  0.465 ms, max 0.593 ms. The new cold sample is small, so the earlier cold p95 overrun remains
  open. The real-module test covers a remote URL automatically.

### Checkpoint on 2026-10-06

- The user confirmed that the cold generic action opens the inspector and that an item in Dolphin's
  non-local `trash:/` location shows no LinuxGitShell action. The manual checklist is complete.
- Native `actions()` timing in a development Dolphin process: 22 single-item generic decisions,
  p50 0.129 ms, p95 0.589 ms, max 2.028 ms. Twenty decisions came from 20 new files in a
  disposable repository: p50 0.125 ms, p95 0.276 ms, max 0.589 ms. The user repeated one click;
  the log contains one additional warm submenu during that sequence. The 22-sample cold p95 meets
  the 2 ms target. Its first-call maximum and the older nine-sample overrun remain recorded for
  context. See [`docs/dolphin-context-menu.md`](docs/dolphin-context-menu.md#native-checkpoint-on-2026-10-05).
- Four warm menus in this process had p50 0.247 ms, p95/max 0.582 ms; the larger 2026-10-05 warm
  series had p95 0.465 ms over 29 samples. The menu screenshot is in `docs/images/` and still
  needs to be attached to #16.

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

- `RepositoryStatusIndex`: model, limits, classification, directory summaries and deterministic tests.

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
- The first measured native menu in the latest process took 2.028 ms, above the 2 ms p95 target
  on its own; p95 across 22 generic menus was 0.589 ms. An earlier nine-sample series had p95
  4.427 ms. First-call variation remains visible, though no synchronous Git or IPC runs in
  `actions()`.

## Next steps

1. Complete and verify the status-index model against the revised overlay contract.
2. Implement bounded process capture and tracked inventory, then the daemon status provider.
3. Implement overlay transport/plugin and native verification before `v0.2.0`.
