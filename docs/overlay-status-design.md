# Icon Overlay Status Design

Status: proposed for Phase 3 (`v0.2.0`, part 2). Nothing described here is implemented yet.
Builds on [`repository-context-cache.md`](repository-context-cache.md) and the experimental
context service in `daemon/`.

## Problem and constraints

Dolphin asks an overlay plugin for icons while it paints a directory view. The KF6 API is
`KOverlayIconPlugin` (KIOCore):

```cpp
virtual QStringList getOverlays(const QUrl &item) = 0;   // synchronous, called per item
Q_SIGNALS: void overlaysChanged(const QUrl &url, const QStringList &overlays);
```

`getOverlays()` is called on Dolphin's main thread for every visible item, so it has the same
rules as `actions()`: no Git, no filesystem walk, no synchronous IPC. The asynchronous
`overlaysChanged` signal is the supported way to deliver a state that was not known at paint time.

Per-item `git status` is never acceptable. One `git status` per repository produces the state of
every item beneath it.

## Proposed boundary

| Piece | Location | Responsibility |
| --- | --- | --- |
| `RepositoryStatusIndex` | `libs/repositorycontext/` (Git-free) | Bounded, per-repository map from relative path to a combined state plus an aggregated state for each ancestor directory. |
| Status provider | `daemon/` | Runs `GitStatusReader` per observed repository, feeds the index, debounces refreshes, emits change signals. |
| Overlay plugin | `integrations/dolphin/overlays/` | Thin `KOverlayIconPlugin`: in-memory lookup in a process-wide client cache; asynchronous subscription to daemon updates. |

The plugin follows the existing context-client pattern: a process-wide object that performs no
D-Bus work in its constructor, uses `QDBusConnection::asyncCall()` and a sender-less signal
subscription, and drops its cache when the service generation changes.

## State model

The detailed model keeps index and work-tree states separate (as `GitStatusEntry` already does).
Overlays use a reduced, ordered presentation state:

`Conflict` > `Modified` > `Added` > `Deleted` > `Untracked` > `Ignored` > `Clean` > `Unknown`

- `Unknown` means "no snapshot yet". It produces no overlay; it must never be drawn as `Clean`.
- Renamed, copied, and type-changed entries map to `Modified`/`Added` by the destination path.
- A submodule shows `Modified` when its commit or tracked content changed, otherwise `Clean`.
- `Deleted` and `Ignored` are included in the model from the start but their icons ship after the
  five MVP states (clean, modified, added, conflict, untracked), as in the roadmap.
- Combined index+work-tree states resolve by precedence for the icon; the model keeps both sides so
  the Working Tree view (Phase 5) can show the detail.

### Directories

A directory's state is the highest-precedence state among its descendants, excluding `Ignored`
and `Untracked` unless configured. Aggregation is computed once per refresh while walking the
status entries (each entry updates its ancestors up to the repository root, O(depth)), not by
scanning the tree on request. Recursive propagation is a setting; when disabled, directories show
only their own state (`Clean`, or `Untracked` for untracked directories) and the aggregation work
is skipped.

## Daemon behavior

1. **Observed set.** A repository becomes observed when a client asks for overlays under it
   (first `RequestOverlays`) or the context service resolves it. Observation expires after an idle
   period, with a hard cap on observed repositories.
2. **Refresh.** One `git status --porcelain=v2 -z --no-optional-locks` per repository through
   `GitStatusReader`, at most N concurrent refreshes (start with 2). Ignored files are requested
   only if the `Ignored` overlay is enabled. Untracked handling honors the repository's own
   `status.showUntrackedFiles`.
3. **Triggers.** Phase 3 starts with explicit triggers only: first observation, a staleness timer
   while a client is viewing the repository, and `RefreshRepository` from LinuxGitShell itself.
   Filesystem watchers belong to Phase 4 and plug into the same debounced `scheduleRefresh()`; the
   Phase 3 exit criterion "updates without restarting Dolphin" is met with the timer, and
   "an edit updates the overlay via watcher" is met in Phase 4.
4. **Diff and notify.** After a refresh the provider diffs the new index against the previous
   one and emits changes only for paths whose presentation state changed, batched.
5. **Large repositories.** Status output is size-capped. Past the cap the repository is marked
   `Degraded`: only repository-level state is reported, and the UI does not claim cleanliness.
6. **Safe exclusions.** Repositories under `/proc`, `/sys`, `/dev`, `/run` and non-local
   filesystems are not observed.

## D-Bus contract (experimental)

Extends the experimental namespace, not the stable `Daemon1` API; see
[`dbus-api-policy.md`](dbus-api-policy.md).

- `RequestOverlays(as paths) -> t generation` — queues observation/lookup; returns immediately.
- `OverlaysChanged(a(ss) changes, t generation)` — batched `(path, state)` pairs, state being one
  of the presentation names above. Clients ignore unknown states.

Path keys follow the same lexical normalization as the context cache. Bounds mirror the context
service: at most 256 paths per request; a global cap on queued paths.

## Plugin behavior

1. `getOverlays(url)` rejects non-local URLs, normalizes the key, and performs a hash lookup in a
   bounded LRU. Warm: returns the icon name list for the state. Cold: returns an empty list and
   queues the key (zero-delay timer) for `RequestOverlays`.
2. On `OverlaysChanged`, the client updates its cache and emits `overlaysChanged(url, icons)` for
   each changed key that it currently tracks.
3. Absence or restart of the daemon degrades to "no overlays". The plugin never retries
   synchronously, backs off after repeated failures, and never affects Dolphin's responsiveness.
4. A disabled setting means `getOverlays()` returns immediately and the client never subscribes or
   sends requests: the extra cost is zero beyond the plugin load.

Paths, repository contents and Git output are never logged by default.

## Icons

Own, freely licensed icons for the five MVP states, designed for 16, 22, and 32 px, HiDPI, and
light and dark themes, using icon names in the freedesktop `emblem-*` style so themes can
override them. States must not rely on color alone (distinct glyph shapes). Source, license, and
attribution for each asset go in `docs/licensing.md`. Icons are added to `icons/` only when the
first real asset exists.

## Open questions

1. **Does Dolphin on Wayland call `getOverlays()` again after `overlaysChanged`?** Verify
   natively, including in the details and compact views, and with the folder-icon scaling
   Dolphin applies to overlays.
2. **Ownership of staleness.** Is a fixed timer acceptable until watchers exist, or should Phase 3
   ship with an optional refresh hook from the application only?
3. **Directory aggregation default.** Spec calls for configurability; default on or off? Proposal:
   on, with the aggregation cost measured on a 100k-file repository.
4. **Status cost on large repos.** Measure `git status` time with and without untracked-cache and
   fsmonitor on a monorepo-sized fixture before choosing the staleness interval.

## Verification plan

Automated:

- `RepositoryStatusIndex` unit tests: state precedence, directory aggregation, renames, conflicts,
  submodules, eviction bounds, `Unknown` never reported as `Clean`, and unusual paths.
- Provider integration tests against temporary repositories covering clean, modified, staged,
  untracked, ignored, renamed, and conflicted states; debounced refresh; batched diff output.
- D-Bus tests on a private bus: request/changed round trip, generation change, daemon absence.
- Real-module plugin test (as for the context plugin): building overlays for 10,000 items performs
  no Git execution and no D-Bus call before returning to the event loop.

Native (recorded in `STATUS.md` before closing the phase): the five states render correctly in
icons, details and compact views; edit, create, delete, stage and resolve updates are visible
without restarting Dolphin; p50/p95/max of `getOverlays()` with a cold and a warm cache; a deep
tree and a large repository; an external mount; and a disabled setting removing the cost.

## Proposed increments

1. `RepositoryStatusIndex` model with tests (no Git, no IPC).
2. Daemon status provider and experimental D-Bus methods with integration tests.
3. Overlay plugin with client cache and the first icon set.
4. Native verification, settings toggle for overlays and recursion, documentation, `v0.2.0`.
